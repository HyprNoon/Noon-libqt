#include "ScreenTimeManager.hpp"

#include <QJsonDocument>
#include <QSaveFile>
#include <QFile>
#include <QDir>
#include <QDate>
#include <QStandardPaths>
#include <QProcess>
#include "HyprBridge.hpp"

ScreenTimeManager::ScreenTimeManager(QObject *parent) : QObject(parent)
{
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(m_saveMs);
    connect(&m_saveTimer, &QTimer::timeout, this, &ScreenTimeManager::save);

    m_checkpointTimer.setInterval(60000);
    connect(&m_checkpointTimer, &QTimer::timeout, this, &ScreenTimeManager::checkpoint);

    m_checkpointTimer.start();
}

ScreenTimeManager::~ScreenTimeManager() = default;

void ScreenTimeManager::init(QObject *parent, const QString &dbPath)
{
    setParent(parent);
    if (!dbPath.isEmpty())
        setDbPath(dbPath);
}

void ScreenTimeManager::setDbPath(const QString &path)
{
    if (m_dbPath == path) return;
    m_dbPath = path;
    QDir().mkpath(m_dbPath);
    emit dbPathChanged();
    loadToday();
    prime();
}

void ScreenTimeManager::setBridge(HyprBridge *bridge)
{
    if (m_bridge == bridge) return;
    if (m_bridge)
        disconnect(m_bridge, &HyprBridge::activeWindowChanged,
                   this, &ScreenTimeManager::onActiveWindow);
    m_bridge = bridge;
    if (m_bridge)
        connect(m_bridge, &HyprBridge::activeWindowChanged,
                this, &ScreenTimeManager::onActiveWindow);
    emit bridgeChanged();
}

void ScreenTimeManager::setSaveInterval(int ms)
{
    if (m_saveMs == ms) return;
    m_saveMs = ms;
    m_saveTimer.setInterval(ms);
    emit saveIntervalChanged();
}

QJsonArray ScreenTimeManager::getDay(const QString &date) const
{
    QFile f(m_dbPath + "/" + date + ".json");
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).array();
}

QJsonArray ScreenTimeManager::getDayTotals(const QString &date) const
{
    auto arr = getDay(date);
    QMap<QString, int> totals;
    for (const auto &v : arr) {
        auto o = v.toObject();
        totals[o.value("class").toString()] +=
            o.value("end").toInt() - o.value("start").toInt();
    }
    QJsonArray out;
    for (auto it = totals.begin(); it != totals.end(); ++it) {
        QJsonObject o;
        o["class"] = it.key();
        o["timeSeconds"] = it.value();
        out.append(o);
    }
    return out;
}

QJsonArray ScreenTimeManager::getDayTimeline(const QString &date, int fromSec, int toSec) const
{
    auto arr = getDay(date);
    QMap<QString, int> totals;
    for (const auto &v : arr) {
        auto o = v.toObject();
        int s = o.value("start").toInt();
        int e = o.value("end").toInt();
        int overlapStart = qMax(s, fromSec);
        int overlapEnd   = qMin(e, toSec);
        if (overlapEnd > overlapStart)
            totals[o.value("class").toString()] += overlapEnd - overlapStart;
    }
    QJsonArray out;
    for (auto it = totals.begin(); it != totals.end(); ++it) {
        QJsonObject o;
        o["class"] = it.key();
        o["timeSeconds"] = it.value();
        out.append(o);
    }
    return out;
}

QString ScreenTimeManager::todayStr() const
{
    return QDate::currentDate().toString(Qt::ISODate);
}

int ScreenTimeManager::secsSinceMidnight() const
{
    auto now = QDateTime::currentDateTime();
    return now.time().hour() * 3600 + now.time().minute() * 60 + now.time().second();
}

bool ScreenTimeManager::hasDb() const
{
    return !m_dbPath.isEmpty();
}

void ScreenTimeManager::prime()
{
    auto *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exit, QProcess::ExitStatus status) {
        if (exit != 0 || status != QProcess::NormalExit) { proc->deleteLater(); return; }
        auto doc = QJsonDocument::fromJson(proc->readAllStandardOutput());
        proc->deleteLater();
        if (!doc.isObject()) return;
        QString cls = doc.object().value("class").toString();
        if (!cls.isEmpty()) {
            m_currentClass = cls;
            m_sessionStart = QDateTime::currentSecsSinceEpoch();
        }
    });
    proc->start("hyprctl", {"-j", "activewindow"});
}

void ScreenTimeManager::checkpoint()
{
    flushSession();
    QString nowDate = todayStr();
    if (m_today != nowDate) {
        save();
        m_sessions = QJsonArray{};
        m_today = nowDate;
        loadToday();
    } else {
        save();
    }
    rebuild();
}

void ScreenTimeManager::flushSession()
{
    if (m_currentClass.isEmpty() || m_sessionStart <= 0) return;
    qint64 now = QDateTime::currentSecsSinceEpoch();
    qint64 delta = now - m_sessionStart;
    if (delta <= 0) return;
    m_sessionStart = now;

    int end = secsSinceMidnight();
    int start = end - delta;
    if (start < 0) start = 0;

    QJsonObject session;
    session["class"] = m_currentClass;
    session["start"] = start;
    session["end"]   = end;
    m_sessions.append(session);
}

void ScreenTimeManager::onActiveWindow(const QString &cls, const QString &)
{
    flushSession();
    m_currentClass = cls;
    m_sessionStart = QDateTime::currentSecsSinceEpoch();
    rebuild();
    m_saveTimer.start();
}

void ScreenTimeManager::rebuild()
{
    QMap<QString, int> totals;
    for (const auto &v : m_sessions) {
        auto o = v.toObject();
        totals[o.value("class").toString()] +=
            o.value("end").toInt() - o.value("start").toInt();
    }
    QJsonArray arr;
    for (auto it = totals.begin(); it != totals.end(); ++it) {
        QJsonObject o;
        o["class"] = it.key();
        o["timeSeconds"] = it.value();
        arr.append(o);
    }
    m_appTimes = arr;
    emit appTimesChanged();
    emit sessionsChanged();
}

void ScreenTimeManager::loadToday()
{
    if (!hasDb()) return;
    m_today = todayStr();
    QFile f(m_dbPath + "/" + m_today + ".json");
    if (f.open(QIODevice::ReadOnly))
        m_sessions = QJsonDocument::fromJson(f.readAll()).array();
    rebuild();
}

void ScreenTimeManager::save()
{
    if (!hasDb()) return;
    QSaveFile f(m_dbPath + "/" + m_today + ".json");
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(m_sessions).toJson());
        f.commit();
    }
}
