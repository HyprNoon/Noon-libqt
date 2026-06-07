#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QProcessEnvironment>
#include <QByteArray>

class HyprBridge : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QJsonArray   windowList            READ windowList            NOTIFY windowListChanged)
    Q_PROPERTY(QJsonObject  windowByAddress       READ windowByAddress       NOTIFY windowByAddressChanged)
    Q_PROPERTY(QJsonArray   workspaces            READ workspaces            NOTIFY workspacesChanged)
    Q_PROPERTY(QJsonObject  workspaceById         READ workspaceById         NOTIFY workspaceByIdChanged)
    Q_PROPERTY(QJsonObject  activeWorkspace       READ activeWorkspace       NOTIFY activeWorkspaceChanged)
    Q_PROPERTY(QJsonArray   monitors              READ monitors              NOTIFY monitorsChanged)
    Q_PROPERTY(QString      currentKeyboardLayout READ currentKeyboardLayout NOTIFY currentKeyboardLayoutChanged)

public:
    explicit HyprBridge(QObject *parent = nullptr) : QObject(parent)
    {
        const auto env = QProcessEnvironment::systemEnvironment();
        m_socketPath = env.value("XDG_RUNTIME_DIR") + "/hypr/" + env.value("HYPRLAND_INSTANCE_SIGNATURE") + "/";

        setupSocket(&m_winSocket, &m_winBuffer, &m_winReady, "j/clients", [this](const QJsonArray &arr) { setWindowList(arr); });
        setupSocket(&m_wsSocket,  &m_wsBuffer,  &m_wsReady,  "j/workspaces", [this](const QJsonArray &arr) { setWorkspaces(arr); });
        setupSocket(&m_monSocket, &m_monBuffer, &m_monReady, "j/monitors", [this](const QJsonArray &arr) {
            m_monitors = arr;
            emit monitorsChanged();
        });

        connect(&m_eventSocket, &QLocalSocket::readyRead,    this, &HyprBridge::onEventData);
        connect(&m_eventSocket, &QLocalSocket::disconnected, this, [this]() {
            m_eventBuffer.clear();
            m_eventSocket.connectToServer(m_socketPath + ".socket2.sock");
        });

        refetch(&m_winSocket, &m_winBuffer, &m_winReady);
        refetch(&m_wsSocket,  &m_wsBuffer,  &m_wsReady);
        refetch(&m_monSocket, &m_monBuffer, &m_monReady);
        initDevices();
        m_eventSocket.connectToServer(m_socketPath + ".socket2.sock");
    }

    QJsonArray  windowList()            const { return m_windowList; }
    QJsonObject windowByAddress()       const { return m_windowByAddress; }
    QJsonArray  workspaces()            const { return m_workspaces; }
    QJsonObject workspaceById()         const { return m_workspaceById; }
    QJsonObject activeWorkspace()       const { return m_activeWorkspace; }
    QJsonArray  monitors()              const { return m_monitors; }
    QString     currentKeyboardLayout() const { return m_currentKeyboardLayout; }

signals:
    void windowListChanged();
    void windowByAddressChanged();
    void workspacesChanged();
    void workspaceByIdChanged();
    void activeWorkspaceChanged();
    void monitorsChanged();
    void currentKeyboardLayoutChanged();
    void activeWindowChanged(const QString &className, const QString &title);

private:
    QString      m_socketPath;
    QJsonArray   m_windowList;
    QJsonObject  m_windowByAddress;
    QJsonArray   m_workspaces;
    QJsonObject  m_workspaceById;
    QJsonObject  m_activeWorkspace;
    QJsonArray   m_monitors;
    QString      m_currentKeyboardLayout;

    QLocalSocket m_winSocket, m_wsSocket, m_monSocket, m_devSocket, m_eventSocket;
    QByteArray   m_winBuffer, m_wsBuffer, m_monBuffer, m_devBuffer, m_eventBuffer;
    bool         m_winReady = false, m_wsReady = false, m_monReady = false, m_devReady = false;

    using ArrayCallback = std::function<void(const QJsonArray &)>;

    void setupSocket(QLocalSocket *sock, QByteArray *buf, bool *ready, const QByteArray &cmd, ArrayCallback cb)
    {
        connect(sock, &QLocalSocket::connected, this, [sock, buf, ready, cmd]() {
            *ready = true;
            *buf   = {};
            sock->write(cmd);
            sock->flush();
        });
        connect(sock, &QLocalSocket::readyRead,    this, [sock, buf]() { *buf += sock->readAll(); });
        connect(sock, &QLocalSocket::disconnected, this, [buf, ready, cb]() {
            if (!*ready) return;
            *ready = false;
            const auto doc = QJsonDocument::fromJson(std::exchange(*buf, {}));
            if (doc.isArray()) cb(doc.array());
        });
    }

    void initDevices()
    {
        m_devReady  = false;
        m_devBuffer = {};
        m_devSocket.abort();

        connect(&m_devSocket, &QLocalSocket::connected, this, [this]() {
            m_devReady  = true;
            m_devBuffer = {};
            m_devSocket.write("j/devices");
            m_devSocket.flush();
        }, Qt::SingleShotConnection);

        connect(&m_devSocket, &QLocalSocket::readyRead, this, [this]() {
            m_devBuffer += m_devSocket.readAll();
        }, Qt::SingleShotConnection);

        connect(&m_devSocket, &QLocalSocket::disconnected, this, [this]() {
            if (!m_devReady) return;
            m_devReady = false;
            const auto doc = QJsonDocument::fromJson(std::exchange(m_devBuffer, {}));
            if (!doc.isObject()) return;
            const auto keyboards = doc.object().value("keyboards").toArray();
            for (const auto &v : keyboards) {
                const auto kb = v.toObject();
                if (kb.value("main").toBool()) {
                    m_currentKeyboardLayout = kb.value("active_keymap").toString();
                    emit currentKeyboardLayoutChanged();
                    return;
                }
            }
            if (!keyboards.isEmpty()) {
                m_currentKeyboardLayout = keyboards.first().toObject().value("active_keymap").toString();
                emit currentKeyboardLayoutChanged();
            }
        }, Qt::SingleShotConnection);

        m_devSocket.connectToServer(m_socketPath + ".socket.sock");
    }

    void refetch(QLocalSocket *sock, QByteArray *buf, bool *ready)
    {
        *ready = false;
        buf->clear();
        sock->abort();
        sock->connectToServer(m_socketPath + ".socket.sock");
    }

    void setWindowList(const QJsonArray &arr)
    {
        m_windowList      = arr;
        m_windowByAddress = {};
        for (const auto &v : arr) {
            const auto o = v.toObject();
            m_windowByAddress.insert(o.value("address").toString(), o);
        }
        emit windowListChanged();
        emit windowByAddressChanged();
    }

    void setWorkspaces(const QJsonArray &arr)
    {
        m_workspaces    = arr;
        m_workspaceById = {};
        for (const auto &v : arr) {
            const auto o = v.toObject();
            m_workspaceById.insert(QString::number(o.value("id").toInt()), o);
        }
        emit workspacesChanged();
        emit workspaceByIdChanged();
    }

    void handleEvent(const QString &event, const QString &data)
    {
        if (event == "openwindow" || event == "closewindow" || event == "movewindow")
            refetch(&m_winSocket, &m_winBuffer, &m_winReady);
        else if (event == "createworkspace" || event == "destroyworkspace")
            refetch(&m_wsSocket, &m_wsBuffer, &m_wsReady);
        else if (event == "monitoradded" || event == "monitorremoved")
            refetch(&m_monSocket, &m_monBuffer, &m_monReady);
        else if (event == "workspace") {
            const QString key = data.trimmed();
            bool ok;
            const int wsId = key.toInt(&ok);
            if (m_workspaceById.contains(key))
                m_activeWorkspace = m_workspaceById.value(key).toObject();
            else if (ok)
                m_activeWorkspace = QJsonObject{{ "id", wsId }, { "name", key }};
            emit activeWorkspaceChanged();
        } else if (event == "activewindow") {
            int sep = data.indexOf(',');
            if (sep != -1)
                emit activeWindowChanged(data.left(sep).trimmed(), data.mid(sep + 1).trimmed());
        } else if (event == "activelayout") {
            const int sep = data.indexOf(',');
            if (sep != -1) {
                m_currentKeyboardLayout = data.mid(sep + 1);
                emit currentKeyboardLayoutChanged();
            }
        }
    }

    void onEventData()
    {
        m_eventBuffer += m_eventSocket.readAll();
        int nl;
        while ((nl = m_eventBuffer.indexOf('\n')) != -1) {
            const QString line = QString::fromUtf8(m_eventBuffer.left(nl));
            m_eventBuffer.remove(0, nl + 1);
            const int sep = line.indexOf(">>");
            if (sep != -1)
                handleEvent(line.left(sep), line.mid(sep + 2));
        }
    }
};
