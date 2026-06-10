#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QMap>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QThread>
#include <QtQml/qqmlregistration.h>

#include <csignal>
#include <chrono>
#include <unistd.h>
#include <pwd.h>

class TaskManager : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QVariantList processes READ processes NOTIFY processesChanged)
    Q_PROPERTY(int processCount READ processCount NOTIFY processCountChanged)

public:
    explicit TaskManager(QObject *parent = nullptr)
        : QObject(parent)
    {
        connect(&m_timer, &QTimer::timeout, this, &TaskManager::refresh);
        m_timer.setInterval(2000);
        m_timer.start();
    }

    ~TaskManager() { m_timer.stop(); }

    QVariantList processes() const { return m_processes; }
    int processCount() const { return m_processCount; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool terminate(qint64 pid) { return kill(pid, SIGTERM); }
    Q_INVOKABLE bool kill(qint64 pid, int sig = SIGKILL) {
        return ::kill(static_cast<pid_t>(pid), sig) == 0;
    }

signals:
    void processesChanged();
    void processCountChanged();

private:
    QTimer m_timer;
    QVariantList m_processes;
    int m_processCount = 0;
    int m_cpuCount = QThread::idealThreadCount();

    using TimePoint = std::chrono::steady_clock::time_point;
    TimePoint m_lastRefresh = std::chrono::steady_clock::now();

    struct ProcCpu {
        unsigned long long utime = 0;
        unsigned long long stime = 0;
    };
    QMap<qint64, ProcCpu> m_prevProcCpu;
};

inline QString readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(f.readAll());
}

inline void TaskManager::refresh()
{
    auto now = std::chrono::steady_clock::now();
    auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(now - m_lastRefresh).count();
    m_lastRefresh = now;

    auto hertz = static_cast<double>(sysconf(_SC_CLK_TCK));
    auto elapsedTicks = static_cast<double>(elapsedUs) * hertz / 1000000.0;
    if (elapsedTicks < 0.001) elapsedTicks = 0.001;

    QDir procDir("/proc");
    auto entries = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    QVariantList list;
    QMap<qint64, ProcCpu> curCpu;

    for (const auto &entry : entries) {
        bool ok = false;
        pid_t pid = entry.toInt(&ok);
        if (!ok || pid <= 0) continue;

        auto stat = readFile(QStringLiteral("/proc/%1/stat").arg(entry));
        if (stat.isEmpty()) continue;

        auto closeParen = stat.lastIndexOf(')');
        if (closeParen < 0) continue;

        auto afterParen = stat.mid(closeParen + 2);
        auto fields = afterParen.split(' ');

        if (fields.size() < 15) continue;

        char state = fields[0].isEmpty() ? '?' : fields[0].at(0).toLatin1();
        auto ppid = fields[1].toLongLong();
        auto utime = fields[11].toULongLong();
        auto stime = fields[12].toULongLong();

        auto nameStart = stat.indexOf('(') + 1;
        auto name = stat.mid(nameStart, closeParen - nameStart);

        auto pid64 = static_cast<qint64>(pid);

        double cpuPct = 0.0;
        auto prevIt = m_prevProcCpu.constFind(pid64);
        if (prevIt != m_prevProcCpu.constEnd()) {
            auto dUtime = utime - prevIt->utime;
            auto dStime = stime - prevIt->stime;
            cpuPct = (dUtime + dStime) / elapsedTicks * 100.0 / m_cpuCount;
            if (cpuPct < 0.0) cpuPct = 0.0;
        }

        curCpu[pid64] = { utime, stime };

        auto status = readFile(QStringLiteral("/proc/%1/status").arg(entry));
        auto procName = name;
        QString user = "?";
        unsigned long vmRssKB = 0;
        int threads = 0;

        for (const auto &line : status.split('\n')) {
            if (line.startsWith("Name:\t"))
                procName = line.mid(6).trimmed();
            else if (line.startsWith("VmRSS:\t")) {
                auto val = line.mid(7).trimmed();
                auto space = val.indexOf(' ');
                if (space > 0) vmRssKB = val.left(space).toULong();
            } else if (line.startsWith("Threads:\t"))
                threads = line.mid(9).trimmed().toInt();
            else if (line.startsWith("Uid:\t")) {
                auto uidStr = line.mid(5).trimmed().section('\t', 0, 0);
                auto uid = uidStr.toUInt();
                struct passwd *pw = getpwuid(uid);
                user = pw ? QString::fromLatin1(pw->pw_name) : QString::number(uid);
            }
        }

        auto cmdlineRaw = readFile(QStringLiteral("/proc/%1/cmdline").arg(entry));
        QString cmdline;
        if (!cmdlineRaw.isEmpty()) {
            auto parts = cmdlineRaw.split('\0');
            for (int i = 0; i < parts.size(); ++i) {
                if (!parts[i].isEmpty()) {
                    if (!cmdline.isEmpty()) cmdline += ' ';
                    cmdline += parts[i];
                }
            }
        }

        QVariantMap p;
        p["pid"]         = pid64;
        p["ppid"]        = ppid;
        p["name"]        = procName;
        p["command"]     = cmdline.isEmpty() ? procName : cmdline;
        p["state"]       = QString(QChar::fromLatin1(state));
        p["cpuUsage"]    = qRound(cpuPct * 10.0) / 10.0;
        p["memoryUsage"] = static_cast<double>(vmRssKB) * 1024.0;
        p["threads"]     = threads;
        p["user"]        = user;

        list.append(p);
    }

    m_prevProcCpu = std::move(curCpu);
    m_processes   = std::move(list);
    m_processCount = m_processes.size();

    emit processesChanged();
    emit processCountChanged();
}
