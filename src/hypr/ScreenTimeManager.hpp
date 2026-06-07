#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>

class HyprBridge;

class ScreenTimeManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString     dbPath        READ dbPath        WRITE setDbPath        NOTIFY dbPathChanged)
    Q_PROPERTY(HyprBridge* bridge        READ bridge        WRITE setBridge        NOTIFY bridgeChanged)
    Q_PROPERTY(QJsonArray  appTimes      READ appTimes      NOTIFY appTimesChanged)
    Q_PROPERTY(QJsonArray  sessions      READ sessions      NOTIFY sessionsChanged)
    Q_PROPERTY(int         saveInterval  READ saveInterval  WRITE setSaveInterval  NOTIFY saveIntervalChanged)

public:
    explicit ScreenTimeManager(QObject *parent = nullptr);
    ~ScreenTimeManager() override;

    QString     dbPath()       const { return m_dbPath; }
    HyprBridge *bridge()       const { return m_bridge; }
    QJsonArray  appTimes()     const { return m_appTimes; }
    QJsonArray  sessions()     const { return m_sessions; }
    int         saveInterval() const { return m_saveMs; }

    void setDbPath(const QString &path);
    void setBridge(HyprBridge *bridge);
    void setSaveInterval(int ms);

    Q_INVOKABLE QJsonArray getDay(const QString &date) const;
    Q_INVOKABLE QJsonArray getDayTotals(const QString &date) const;
    Q_INVOKABLE QJsonArray getDayTimeline(const QString &date, int fromSec, int toSec) const;

signals:
    void appTimesChanged();
    void sessionsChanged();
    void bridgeChanged();
    void dbPathChanged();
    void saveIntervalChanged();

private:
    QString     m_dbPath;
    HyprBridge *m_bridge = nullptr;
    QJsonArray  m_sessions;
    QJsonArray  m_appTimes;
    QString     m_today;
    QString     m_currentClass;
    qint64      m_sessionStart = 0;
    int         m_saveMs = 5000;
    QTimer      m_saveTimer;
    QTimer      m_checkpointTimer;

    QString todayStr() const;
    int secsSinceMidnight() const;
    bool hasDb() const;

    void prime();
    void checkpoint();
    void flushSession();
    void onActiveWindow(const QString &cls, const QString &title);
    void rebuild();
    void loadToday();
    void save();
};
