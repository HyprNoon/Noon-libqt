#pragma once
#include <MpvAbstractItem>
#include <MpvController>
#include <QUrl>
#include <QVariantMap>

class MpvPlayer : public MpvAbstractItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(MpvPlayer)
    Q_PROPERTY(QUrl        source    READ source    WRITE setSource    NOTIFY sourceChanged)
    Q_PROPERTY(bool        autoplay  READ autoplay  WRITE setAutoplay  NOTIFY autoplayChanged)
    Q_PROPERTY(bool        playing   READ playing                      NOTIFY playingChanged)
    Q_PROPERTY(bool        loop      READ loop      WRITE setLoop      NOTIFY loopChanged)
    Q_PROPERTY(bool        mute      READ mute      WRITE setMute      NOTIFY muteChanged)
    Q_PROPERTY(int         framerate READ framerate WRITE setFramerate NOTIFY framerateChanged)
    Q_PROPERTY(QVariantMap mpvProps  READ mpvProps  WRITE setMpvProps  NOTIFY mpvPropsChanged)

public:
    explicit MpvPlayer(QQuickItem *parent = nullptr) : MpvAbstractItem(parent) {
        observeProperty(QStringLiteral("pause"), MPV_FORMAT_FLAG);
        connect(mpvController(), &MpvController::propertyChanged, this, [this](const QString &p, const QVariant &v) {
            if (p == QLatin1String("pause")) {
                m_playing = !v.toBool();
                Q_EMIT playingChanged();
            }
        }, Qt::QueuedConnection);
    }

    void componentComplete() override {
        MpvAbstractItem::componentComplete();
        setProperty(QStringLiteral("vf"), QStringLiteral("fps=%1").arg(m_framerate));
        applyMpvProps();
        if (!m_source.isEmpty())
            QTimer::singleShot(100, this, &MpvPlayer::loadSource);
    }

    QUrl        source()    const { return m_source; }
    bool        autoplay()  const { return m_autoplay; }
    bool        playing()   const { return m_playing; }
    bool        loop()      const { return m_loop; }
    bool        mute()      const { return m_mute; }
    int         framerate() const { return m_framerate; }
    QVariantMap mpvProps()  const { return m_mpvProps; }

    void setSource(const QUrl &v) {
        if (m_source == v) return;
        m_source = v;
        Q_EMIT sourceChanged();
        if (isComponentComplete()) loadSource();
    }

    void setAutoplay(bool v)   { if (m_autoplay  == v) return; m_autoplay  = v; Q_EMIT autoplayChanged(); }
    void setLoop(bool v)       { if (m_loop      == v) return; m_loop      = v; setProperty(QStringLiteral("loop-file"), v ? QStringLiteral("inf") : QStringLiteral("no")); Q_EMIT loopChanged(); }
    void setMute(bool v)       { if (m_mute      == v) return; m_mute      = v; setProperty(QStringLiteral("mute"), v); Q_EMIT muteChanged(); }
    void setFramerate(int v)   { if (m_framerate == v) return; m_framerate = v; setProperty(QStringLiteral("vf"), QStringLiteral("fps=%1").arg(v)); Q_EMIT framerateChanged(); }

    void setMpvProps(const QVariantMap &v) {
        if (m_mpvProps == v) return;
        m_mpvProps = v;
        Q_EMIT mpvPropsChanged();
        if (isComponentComplete()) applyMpvProps();
    }

    Q_INVOKABLE void play()           { setProperty(QStringLiteral("pause"), false); }
    Q_INVOKABLE void pause()          { setProperty(QStringLiteral("pause"), true); }
    Q_INVOKABLE void togglePlayback() { setProperty(QStringLiteral("pause"), m_playing); }
    Q_INVOKABLE void stop()           { command({QStringLiteral("stop")}); }
    Q_INVOKABLE void load(const QUrl &url) {
        m_source = url;
        Q_EMIT sourceChanged();
        loadSource();
    }

Q_SIGNALS:
    void sourceChanged();
    void autoplayChanged();
    void playingChanged();
    void loopChanged();
    void muteChanged();
    void framerateChanged();
    void mpvPropsChanged();

private:
    void applyMpvProps() {
        for (auto it = m_mpvProps.cbegin(); it != m_mpvProps.cend(); ++it)
            setProperty(it.key(), it.value());
    }

    void loadSource() {
        command({QStringLiteral("loadfile"),
                 m_source.toString(QUrl::PreferLocalFile),
                 QStringLiteral("replace"),
                 QStringLiteral("-1"),
                 m_autoplay ? QStringLiteral("pause=no") : QStringLiteral("pause=yes")});
    }

    QUrl        m_source;
    QVariantMap m_mpvProps;
    bool        m_playing = false, m_autoplay = false, m_loop = false, m_mute = false;
    int         m_framerate = 24;
};
