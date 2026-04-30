#ifndef RESOURCESWATCHER_H
#define RESOURCESWATCHER_H

#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <QTimer>
#include <QMap>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QTextStream>
#include <qqmlengine.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <sys/ioctl.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm/i915_drm.h>
#include <nvml.h>

class ResourcesWatcher : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantMap stats READ stats NOTIFY statsChanged)
    Q_PROPERTY(int updateInterval READ updateInterval WRITE setUpdateInterval NOTIFY updateIntervalChanged)
    Q_PROPERTY(int diskUpdateInterval READ diskUpdateInterval WRITE setDiskUpdateInterval NOTIFY diskUpdateIntervalChanged)

public:
    explicit ResourcesWatcher(QObject *parent = nullptr);
    ~ResourcesWatcher() override;

    QVariantMap stats() const { return m_stats; }

    int updateInterval() const { return m_updateTimer->interval(); }
    void setUpdateInterval(int ms) {
        if (ms > 0 && ms != updateInterval()) {
            m_updateTimer->setInterval(ms);
            emit updateIntervalChanged();
        }
    }

    int diskUpdateInterval() const { return m_diskTimer->interval(); }
    void setDiskUpdateInterval(int ms) {
        if (ms > 0 && ms != diskUpdateInterval()) {
            m_diskTimer->setInterval(ms);
            emit diskUpdateIntervalChanged();
        }
    }

signals:
    void statsChanged();
    void updateIntervalChanged();
    void diskUpdateIntervalChanged();

private:
    static QVariantMap makeGpuEntry(const QString &vendor, int index, const QString &name);

    void updateStats();
    void updateDiskStats();
    void readCpuStats(double &cpuPercent, double &cpuFreqGhz, double &cpuTotalFreqGhz, double &cpuTemp);
    void readMemoryAndSwapStats(quint64 &memTotal, quint64 &memAvailable, quint64 &swapTotal, quint64 &swapFree);
    QVariantList readGpuStats();
    void readNvidiaGpuStats(QVariantList &gpus);
    void readAmdGpuStats(QVariantList &gpus);
    void readIntelGpuStats(QVariantList &gpus);

    QVariantMap m_stats;
    QTimer *m_updateTimer;
    QTimer *m_diskTimer;
    quint64 m_prevTotal;
    quint64 m_prevIdle;
    QMap<QString, quint64> m_intelPrevBusyNs;
    QMap<QString, quint64> m_intelPrevTotalNs;
};

#endif
