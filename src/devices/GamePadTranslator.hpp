#pragma once
#include <QObject>
#include <QTimer>
#include <QQmlEngine>
#include <SDL.h>

class GamePadTranslator : public QObject {
    Q_OBJECT
    QML_ELEMENT

public:
    enum class Button {
        FaceDown, FaceRight, FaceLeft, FaceUp,
        LB, RB, LS, RS,
        MenuStart, MenuBack,
        DUp, DDown, DLeft, DRight,
        ButtonUnknown
    };
    Q_ENUM(Button)

    enum class Axis {
        LX, LY, RX, RY, LT, RT,
        AxisUnknown
    };
    Q_ENUM(Axis)

    Q_PROPERTY(int     deviceIndex READ deviceIndex WRITE setDeviceIndex NOTIFY deviceConnected)
    Q_PROPERTY(bool    connected   READ connected   NOTIFY deviceConnected)
    Q_PROPERTY(QString name        READ name        NOTIFY deviceConnected)
    Q_PROPERTY(int     axisCount   READ axisCount   NOTIFY deviceConnected)
    Q_PROPERTY(int     buttonCount READ buttonCount NOTIFY deviceConnected)

    Q_PROPERTY(float lx READ lx NOTIFY axisChanged)
    Q_PROPERTY(float ly READ ly NOTIFY axisChanged)
    Q_PROPERTY(float rx READ rx NOTIFY axisChanged)
    Q_PROPERTY(float ry READ ry NOTIFY axisChanged)
    Q_PROPERTY(float lt READ lt NOTIFY axisChanged)
    Q_PROPERTY(float rt READ rt NOTIFY axisChanged)

    Q_PROPERTY(bool faceDown  READ faceDown  NOTIFY buttonChanged)
    Q_PROPERTY(bool faceRight READ faceRight NOTIFY buttonChanged)
    Q_PROPERTY(bool faceLeft  READ faceLeft  NOTIFY buttonChanged)
    Q_PROPERTY(bool faceUp    READ faceUp    NOTIFY buttonChanged)
    Q_PROPERTY(bool lb        READ lb        NOTIFY buttonChanged)
    Q_PROPERTY(bool rb        READ rb        NOTIFY buttonChanged)
    Q_PROPERTY(bool ls        READ ls        NOTIFY buttonChanged)
    Q_PROPERTY(bool rs        READ rs        NOTIFY buttonChanged)
    Q_PROPERTY(bool menuStart READ menuStart NOTIFY buttonChanged)
    Q_PROPERTY(bool menuBack  READ menuBack  NOTIFY buttonChanged)
    Q_PROPERTY(bool dUp       READ dUp       NOTIFY buttonChanged)
    Q_PROPERTY(bool dDown     READ dDown     NOTIFY buttonChanged)
    Q_PROPERTY(bool dLeft     READ dLeft     NOTIFY buttonChanged)
    Q_PROPERTY(bool dRight    READ dRight    NOTIFY buttonChanged)

    explicit GamePadTranslator(QObject *p = nullptr) : QObject(p) {
        static bool sdlReady = false;
        if (!sdlReady) { SDL_Init(SDL_INIT_GAMECONTROLLER); sdlReady = true; }
        connect(&m_timer, &QTimer::timeout, this, &GamePadTranslator::poll);
        m_timer.start(16);
        tryOpen(0);
    }

    ~GamePadTranslator() { if (m_pad) SDL_GameControllerClose(m_pad); }

    int     deviceIndex() const { return m_deviceIndex; }
    bool    connected()   const { return m_pad != nullptr; }
    QString name()        const { return m_name; }
    int     axisCount()   const { return m_axisCount; }
    int     buttonCount() const { return m_buttonCount; }

    float lx() const { return m_axes[0]; }
    float ly() const { return m_axes[1]; }
    float rx() const { return m_axes[2]; }
    float ry() const { return m_axes[3]; }
    float lt() const { return (m_axes[4] + 1.0f) / 2.0f; }
    float rt() const { return (m_axes[5] + 1.0f) / 2.0f; }

    bool faceDown()  const { return m_btns[SDL_CONTROLLER_BUTTON_A];             }
    bool faceRight() const { return m_btns[SDL_CONTROLLER_BUTTON_B];             }
    bool faceLeft()  const { return m_btns[SDL_CONTROLLER_BUTTON_X];             }
    bool faceUp()    const { return m_btns[SDL_CONTROLLER_BUTTON_Y];             }
    bool lb()        const { return m_btns[SDL_CONTROLLER_BUTTON_LEFTSHOULDER];  }
    bool rb()        const { return m_btns[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER]; }
    bool ls()        const { return m_btns[SDL_CONTROLLER_BUTTON_LEFTSTICK];     }
    bool rs()        const { return m_btns[SDL_CONTROLLER_BUTTON_RIGHTSTICK];    }
    bool menuStart() const { return m_btns[SDL_CONTROLLER_BUTTON_START];         }
    bool menuBack()  const { return m_btns[SDL_CONTROLLER_BUTTON_BACK];          }
    bool dUp()       const { return m_btns[SDL_CONTROLLER_BUTTON_DPAD_UP];       }
    bool dDown()     const { return m_btns[SDL_CONTROLLER_BUTTON_DPAD_DOWN];     }
    bool dLeft()     const { return m_btns[SDL_CONTROLLER_BUTTON_DPAD_LEFT];     }
    bool dRight()    const { return m_btns[SDL_CONTROLLER_BUTTON_DPAD_RIGHT];    }

    Q_INVOKABLE float axisValue(int i)   const { return i < m_axisCount   ? m_axes[i] : 0.f;  }
    Q_INVOKABLE bool  buttonValue(int i) const { return i < m_buttonCount ? m_btns[i] : false; }

    void setDeviceIndex(int idx) {
        if (m_pad) { SDL_GameControllerClose(m_pad); m_pad = nullptr; }
        tryOpen(idx);
        emit deviceConnected(m_deviceIndex, m_name, connected());
    }

signals:
    void deviceConnected(int index, const QString &name, bool connected);
    void deviceDisconnected(int index, const QString &name);
    void axisChanged(int index, float value);
    void buttonPressed(int index);
    void buttonReleased(int index);
    void buttonChanged(int index, bool pressed);
    void anyButtonPressed();
    void anyButtonReleased();
    void faceDownPressed();   void faceDownReleased();
    void faceRightPressed();  void faceRightReleased();
    void faceLeftPressed();   void faceLeftReleased();
    void faceUpPressed();     void faceUpReleased();
    void lbPressed();         void lbReleased();
    void rbPressed();         void rbReleased();
    void lsPressed();         void lsReleased();
    void rsPressed();         void rsReleased();
    void menuStartPressed();  void menuStartReleased();
    void menuBackPressed();   void menuBackReleased();
    void dUpPressed();        void dUpReleased();
    void dDownPressed();      void dDownReleased();
    void dLeftPressed();      void dLeftReleased();
    void dRightPressed();     void dRightReleased();
    void leftStickMoved(float x, float y);
    void rightStickMoved(float x, float y);
    void leftTriggerMoved(float value);
    void rightTriggerMoved(float value);
    void buttonEvent(int button, bool pressed);
    void axisEvent(int axis, float value);

private:
    static int toButton(int i) {
        switch ((SDL_GameControllerButton)i) {
        case SDL_CONTROLLER_BUTTON_A:             return int(Button::FaceDown);
        case SDL_CONTROLLER_BUTTON_B:             return int(Button::FaceRight);
        case SDL_CONTROLLER_BUTTON_X:             return int(Button::FaceLeft);
        case SDL_CONTROLLER_BUTTON_Y:             return int(Button::FaceUp);
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  return int(Button::LB);
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return int(Button::RB);
        case SDL_CONTROLLER_BUTTON_LEFTSTICK:     return int(Button::LS);
        case SDL_CONTROLLER_BUTTON_RIGHTSTICK:    return int(Button::RS);
        case SDL_CONTROLLER_BUTTON_START:         return int(Button::MenuStart);
        case SDL_CONTROLLER_BUTTON_BACK:          return int(Button::MenuBack);
        case SDL_CONTROLLER_BUTTON_DPAD_UP:       return int(Button::DUp);
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:     return int(Button::DDown);
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:     return int(Button::DLeft);
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:    return int(Button::DRight);
        default:                                  return int(Button::ButtonUnknown);
        }
    }

    static int toAxis(int i) {
        switch (i) {
        case 0: return int(Axis::LX);
        case 1: return int(Axis::LY);
        case 2: return int(Axis::RX);
        case 3: return int(Axis::RY);
        case 4: return int(Axis::LT);
        case 5: return int(Axis::RT);
        default: return int(Axis::AxisUnknown);
        }
    }

    void tryOpen(int idx) {
        m_deviceIndex = idx;
        m_axisCount = m_buttonCount = 0;
        if (idx < SDL_NumJoysticks() && SDL_IsGameController(idx)) {
            m_pad = SDL_GameControllerOpen(idx);
            if (m_pad) {
                m_name        = SDL_GameControllerName(m_pad);
                auto *joy     = SDL_GameControllerGetJoystick(m_pad);
                m_axisCount   = qMin(SDL_JoystickNumAxes(joy),   MaxAxes);
                m_buttonCount = qMin(SDL_JoystickNumButtons(joy), MaxBtns);
            }
        }
    }

    void poll() {
        SDL_GameControllerUpdate();
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_CONTROLLERDEVICEADDED && !m_pad) {
                tryOpen(m_deviceIndex);
                emit deviceConnected(m_deviceIndex, m_name, connected());
            }
            if (e.type == SDL_CONTROLLERDEVICEREMOVED && m_pad) {
                QString n = m_name; int i = m_deviceIndex;
                SDL_GameControllerClose(m_pad);
                m_pad = nullptr; m_name.clear();
                m_axisCount = m_buttonCount = 0;
                emit deviceDisconnected(i, n);
                emit deviceConnected(i, n, false);
            }
        }

        if (!m_pad) return;

        for (int i = 0; i < m_axisCount; ++i) {
            float v = SDL_GameControllerGetAxis(m_pad, (SDL_GameControllerAxis)i) / 32767.0f;
            if (!qFuzzyCompare(v, m_axes[i])) {
                m_axes[i] = v;
                emit axisChanged(i, v);
                emit axisEvent(toAxis(i), v);
                if (i == 0 || i == 1) emit leftStickMoved(m_axes[0], m_axes[1]);
                if (i == 2 || i == 3) emit rightStickMoved(m_axes[2], m_axes[3]);
                if (i == 4) emit leftTriggerMoved((v + 1.0f) / 2.0f);
                if (i == 5) emit rightTriggerMoved((v + 1.0f) / 2.0f);
            }
        }

        for (int i = 0; i < m_buttonCount; ++i) {
            bool v = SDL_GameControllerGetButton(m_pad, (SDL_GameControllerButton)i);
            if (v != m_btns[i]) {
                m_btns[i] = v;
                emit buttonChanged(i, v);
                emit buttonEvent(toButton(i), v);
                v ? emit buttonPressed(i) : emit buttonReleased(i);
                v ? emit anyButtonPressed() : emit anyButtonReleased();
                emitNamed(i, v);
            }
        }
    }

    void emitNamed(int i, bool v) {
        switch ((SDL_GameControllerButton)i) {
        case SDL_CONTROLLER_BUTTON_A:             v ? emit faceDownPressed()   : emit faceDownReleased();   break;
        case SDL_CONTROLLER_BUTTON_B:             v ? emit faceRightPressed()  : emit faceRightReleased();  break;
        case SDL_CONTROLLER_BUTTON_X:             v ? emit faceLeftPressed()   : emit faceLeftReleased();   break;
        case SDL_CONTROLLER_BUTTON_Y:             v ? emit faceUpPressed()     : emit faceUpReleased();     break;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  v ? emit lbPressed()         : emit lbReleased();         break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: v ? emit rbPressed()         : emit rbReleased();         break;
        case SDL_CONTROLLER_BUTTON_LEFTSTICK:     v ? emit lsPressed()         : emit lsReleased();         break;
        case SDL_CONTROLLER_BUTTON_RIGHTSTICK:    v ? emit rsPressed()         : emit rsReleased();         break;
        case SDL_CONTROLLER_BUTTON_START:         v ? emit menuStartPressed()  : emit menuStartReleased();  break;
        case SDL_CONTROLLER_BUTTON_BACK:          v ? emit menuBackPressed()   : emit menuBackReleased();   break;
        case SDL_CONTROLLER_BUTTON_DPAD_UP:       v ? emit dUpPressed()        : emit dUpReleased();        break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:     v ? emit dDownPressed()      : emit dDownReleased();      break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:     v ? emit dLeftPressed()      : emit dLeftReleased();      break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:    v ? emit dRightPressed()     : emit dRightReleased();     break;
        default: break;
        }
    }

    static constexpr int MaxAxes = 8;
    static constexpr int MaxBtns = 20;

    SDL_GameController *m_pad = nullptr;
    QTimer  m_timer;
    QString m_name;
    int     m_deviceIndex = 0;
    int     m_axisCount   = 0;
    int     m_buttonCount = 0;
    float   m_axes[MaxAxes] = {};
    bool    m_btns[MaxBtns] = {};
};
