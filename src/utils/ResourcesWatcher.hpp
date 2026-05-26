#pragma once
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
    explicit ResourcesWatcher(QObject *parent = nullptr)
        : QObject(parent), m_prevTotal(0), m_prevIdle(0)
    {
        nvmlInit();
        m_updateTimer = new QTimer(this);
        connect(m_updateTimer, &QTimer::timeout, this, &ResourcesWatcher::updateStats);
        m_updateTimer->start(2000);
        m_diskTimer = new QTimer(this);
        connect(m_diskTimer, &QTimer::timeout, this, &ResourcesWatcher::updateDiskStats);
        m_diskTimer->start(600000);
        updateDiskStats();
        updateStats();
    }

    ~ResourcesWatcher() override { nvmlShutdown(); }

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
    QVariantMap m_stats;
    QTimer *m_updateTimer;
    QTimer *m_diskTimer;
    quint64 m_prevTotal;
    quint64 m_prevIdle;
    QMap<QString, quint64> m_intelPrevBusyNs;
    QMap<QString, quint64> m_intelPrevTotalNs;

    static QVariantMap makeGpuEntry(const QString &vendor, int index, const QString &name)
    {
        return QVariantMap{
            {"vendor", vendor}, {"index", index}, {"name", name},
            {"temperature", 0.0}, {"utilization", 0.0}, {"memory_total", 0.0},
            {"memory_used", 0.0}, {"power_draw", 0.0}, {"power_limit", 0.0}
        };
    }

    void updateDiskStats()
    {
        QVariantList disks;
        QFile mountsFile("/proc/mounts");
        if (mountsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(mountsFile.readAll());
            mountsFile.close();
            while (!stream.atEnd()) {
                QString line = stream.readLine();
                if (line.isEmpty()) continue;
                QStringList parts = line.simplified().split(' ');
                if (parts.size() < 3) continue;
                QString dev = parts[0], path = parts[1], type = parts[2];
                bool isPhysical = dev.startsWith("/dev/") || path == "/" || path.startsWith("/run/media") || path.startsWith("/mnt");
                bool isVirtual = (type == "tmpfs" || type == "devtmpfs" || type == "squashfs" || type == "proc" || type == "sysfs" || type == "overlay");
                if (isPhysical && !isVirtual && !dev.startsWith("/dev/loop")) {
                    struct statvfs vfs;
                    if (statvfs(path.toLocal8Bit().constData(), &vfs) == 0) {
                        quint64 total = static_cast<quint64>(vfs.f_blocks) * vfs.f_frsize;
                        quint64 available = static_cast<quint64>(vfs.f_bavail) * vfs.f_frsize;
                        if (total > 0) {
                            disks.append(QVariantMap{
                                {"device", dev}, {"mount", path}, {"type", type},
                                {"total", static_cast<qint64>(total)},
                                {"used", static_cast<qint64>(total - available)},
                                {"free", static_cast<qint64>(available)}
                            });
                        }
                    }
                }
            }
        }
        m_stats["disks"] = disks;
        emit statsChanged();
    }

    void updateStats()
    {
        double cpuP = 0, cpuF = 0, cpuTF = 0, cpuT = 0;
        quint64 memT = 0, memA = 0, swpT = 0, swpF = 0;
        readCpuStats(cpuP, cpuF, cpuTF, cpuT);
        readMemoryAndSwapStats(memT, memA, swpT, swpF);
        QVariantList gpus = readGpuStats();
        QVariantList disks = m_stats["disks"].toList();
        m_stats = QVariantMap{
            {"cpu_percent", cpuP}, {"cpu_freq_ghz", cpuF}, {"cpu_total_freq_ghz", cpuTF}, {"cpu_temp", cpuT},
            {"mem_total", static_cast<qint64>(memT)}, {"mem_available", static_cast<qint64>(memA)},
            {"swap_total", static_cast<qint64>(swpT)}, {"swap_free", static_cast<qint64>(swpF)},
            {"gpus", gpus}, {"disks", disks}
        };
        emit statsChanged();
    }

    void readCpuStats(double &cpuPercent, double &cpuFreqGhz, double &cpuTotalFreqGhz, double &cpuTemp)
    {
        QFile statFile("/proc/stat");
        if (statFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QStringList parts = QString(statFile.readLine()).simplified().split(' ');
            if (parts.size() >= 5) {
                quint64 user = parts[1].toULongLong(), nice = parts[2].toULongLong(),
                        system = parts[3].toULongLong(), idle = parts[4].toULongLong(),
                        iowait = parts.size() > 5 ? parts[5].toULongLong() : 0,
                        total = user + nice + system + idle + iowait;
                if (m_prevTotal != 0) {
                    double dTotal = static_cast<double>(total - m_prevTotal);
                    double dIdle = static_cast<double>(idle - m_prevIdle);
                    if (dTotal > 0) cpuPercent = 100.0 * (dTotal - dIdle) / dTotal;
                }
                m_prevTotal = total; m_prevIdle = idle;
            }
        }
        QFile freqFile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
        if (freqFile.open(QIODevice::ReadOnly | QIODevice::Text))
            cpuFreqGhz = freqFile.readAll().trimmed().toDouble() / 1000000.0;
        QDir cpuDir("/sys/devices/system/cpu");
        const QStringList cores = cpuDir.entryList(QStringList("cpu[0-9]*"), QDir::Dirs);
        double totalKhz = 0.0;
        for (const QString &core : cores) {
            QFile f(cpuDir.filePath(core + "/cpufreq/scaling_max_freq"));
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                double khz = f.readAll().trimmed().toDouble();
                if (khz > 0) totalKhz += khz;
            }
        }
        cpuTotalFreqGhz = totalKhz / 1000000.0;
        QDir hwmonDir("/sys/class/hwmon");
        for (const QString &dir : hwmonDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QString path = "/sys/class/hwmon/" + dir;
            QFile nf(path + "/name");
            if (!nf.open(QIODevice::ReadOnly)) continue;
            QString name = nf.readAll().trimmed();
            if (name == "coretemp" || name == "k10temp" || name == "cpu_thermal" || name == "soc_thermal") {
                QFile tf(path + "/temp1_input");
                if (tf.open(QIODevice::ReadOnly)) {
                    cpuTemp = tf.readAll().trimmed().toDouble() / 1000.0;
                    if (cpuTemp > 0) break;
                }
            }
        }
    }

    void readMemoryAndSwapStats(quint64 &memTotal, quint64 &memAvailable, quint64 &swapTotal, quint64 &swapFree)
    {
        struct sysinfo info;
        if (sysinfo(&info) == 0) {
            quint64 u = info.mem_unit;
            memTotal = static_cast<quint64>(info.totalram) * u;
            memAvailable = static_cast<quint64>(info.freeram + info.bufferram) * u;
            swapTotal = static_cast<quint64>(info.totalswap) * u;
            swapFree = static_cast<quint64>(info.freeswap) * u;
        }
    }

    QVariantList readGpuStats()
    {
        QVariantList gpus;
        readNvidiaGpuStats(gpus);
        readAmdGpuStats(gpus);
        readIntelGpuStats(gpus);
        return gpus;
    }

    void readNvidiaGpuStats(QVariantList &gpus)
    {
        unsigned int deviceCount = 0;
        if (nvmlDeviceGetCount(&deviceCount) == NVML_SUCCESS) {
            for (unsigned int i = 0; i < deviceCount; ++i) {
                nvmlDevice_t device;
                if (nvmlDeviceGetHandleByIndex(i, &device) != NVML_SUCCESS) continue;
                char name[NVML_DEVICE_NAME_BUFFER_SIZE];
                nvmlDeviceGetName(device, name, NVML_DEVICE_NAME_BUFFER_SIZE);
                QVariantMap gpu = makeGpuEntry("nvidia", i, QString::fromLocal8Bit(name));
                nvmlTemperature_t tempInfo;
                tempInfo.sensorType = NVML_TEMPERATURE_GPU;
                if (nvmlDeviceGetTemperatureV(device, &tempInfo) == NVML_SUCCESS)
                    gpu["temperature"] = static_cast<double>(tempInfo.temperature);
                nvmlUtilization_t util;
                if (nvmlDeviceGetUtilizationRates(device, &util) == NVML_SUCCESS)
                    gpu["utilization"] = static_cast<double>(util.gpu);
                nvmlMemory_t mem;
                if (nvmlDeviceGetMemoryInfo(device, &mem) == NVML_SUCCESS) {
                    gpu["memory_total"] = static_cast<double>(mem.total) / 1048576.0;
                    gpu["memory_used"] = static_cast<double>(mem.used) / 1048576.0;
                }
                unsigned int power = 0;
                if (nvmlDeviceGetPowerUsage(device, &power) == NVML_SUCCESS)
                    gpu["power_draw"] = static_cast<double>(power) / 1000.0;
                unsigned int limit = 0;
                if (nvmlDeviceGetEnforcedPowerLimit(device, &limit) == NVML_SUCCESS)
                    gpu["power_limit"] = static_cast<double>(limit) / 1000.0;
                gpus.append(gpu);
            }
        }
    }

    void readAmdGpuStats(QVariantList &gpus)
    {
        QDir drmDir("/sys/class/drm");
        for (const QString &card : drmDir.entryList(QStringList("card[0-9]"), QDir::Dirs | QDir::NoDotAndDotDot)) {
            QString base = "/sys/class/drm/" + card + "/device";
            QFile vf(base + "/vendor");
            if (!vf.open(QIODevice::ReadOnly) || vf.readAll().trimmed() != "0x1002") continue;
            QVariantMap gpu = makeGpuEntry("amd", gpus.size(), "AMD GPU");
            QDir hwb(base + "/hwmon");
            for (const QString &hw : hwb.entryList(QStringList("hwmon*"), QDir::Dirs)) {
                QFile tf(base + "/hwmon/" + hw + "/temp1_input");
                if (tf.open(QIODevice::ReadOnly)) gpu["temperature"] = tf.readAll().trimmed().toDouble() / 1000.0;
                QFile pd(base + "/hwmon/" + hw + "/power1_average");
                if (pd.open(QIODevice::ReadOnly)) gpu["power_draw"] = pd.readAll().trimmed().toDouble() / 1000000.0;
            }
            QFile bf(base + "/gpu_busy_percent");
            if (bf.open(QIODevice::ReadOnly)) gpu["utilization"] = bf.readAll().trimmed().toDouble();
            QFile mtf(base + "/mem_info_vram_total"), muf(base + "/mem_info_vram_used");
            if (mtf.open(QIODevice::ReadOnly)) gpu["memory_total"] = mtf.readAll().trimmed().toDouble() / 1048576.0;
            if (muf.open(QIODevice::ReadOnly)) gpu["memory_used"] = muf.readAll().trimmed().toDouble() / 1048576.0;
            gpus.append(gpu);
        }
    }

    void readIntelGpuStats(QVariantList &gpus)
    {
        QDir driDir("/dev/dri");
        for (const QString &node : driDir.entryList(QStringList("renderD*"), QDir::System | QDir::Files)) {
            int fd = open(("/dev/dri/" + node).toLocal8Bit().constData(), O_RDWR);
            if (fd < 0) continue;
            drmVersionPtr ver = drmGetVersion(fd);
            if (!ver) { close(fd); continue; }
            QString dName = QString::fromLatin1(ver->name, ver->name_len);
            drmFreeVersion(ver);
            if (dName != "i915" && dName != "xe") { close(fd); continue; }
            QString card = QString("card%1").arg(node.mid(7).toInt() - 128);
            QString sBase = "/sys/class/drm/" + card + "/device";
            QVariantMap gpu = makeGpuEntry("intel", gpus.size(), "Intel GPU");
            if (dName == "i915") {
                drm_i915_query q = {}; drm_i915_query_item item = {DRM_I915_QUERY_MEMORY_REGIONS};
                q.num_items = 1; q.items_ptr = reinterpret_cast<__u64>(&item);
                if (ioctl(fd, DRM_IOCTL_I915_QUERY, &q) == 0 && item.length > 0) {
                    QByteArray buf(item.length, 0); item.data_ptr = reinterpret_cast<__u64>(buf.data());
                    if (ioctl(fd, DRM_IOCTL_I915_QUERY, &q) == 0) {
                        auto *info = reinterpret_cast<drm_i915_query_memory_regions *>(buf.data());
                        for (quint32 r = 0; r < info->num_regions; ++r) {
                            const auto &reg = info->regions[r];
                            if (reg.region.memory_class == I915_MEMORY_CLASS_DEVICE || reg.region.memory_class == I915_MEMORY_CLASS_SYSTEM) {
                                gpu["memory_total"] = static_cast<double>(reg.probed_size) / 1048576.0;
                                gpu["memory_used"] = static_cast<double>(reg.probed_size - reg.unallocated_size) / 1048576.0;
                                if (reg.region.memory_class == I915_MEMORY_CLASS_DEVICE) break;
                            }
                        }
                    }
                }
            }
            close(fd);
            QDir hwb(sBase + "/hwmon");
            for (const QString &hw : hwb.entryList(QStringList("hwmon*"), QDir::Dirs)) {
                QFile tf(sBase + "/hwmon/" + hw + "/temp1_input");
                if (tf.open(QIODevice::ReadOnly)) gpu["temperature"] = tf.readAll().trimmed().toDouble() / 1000.0;
                QFile pf(sBase + "/hwmon/" + hw + "/power1_input");
                if (pf.open(QIODevice::ReadOnly)) gpu["power_draw"] = pf.readAll().trimmed().toDouble() / 1000000.0;
            }
            double bD = 0, aD = 0;
            for (const QString &eng : QStringList{"rcs0", "vcs0", "bcs0"}) {
                QFile bf(sBase + "/engine/" + eng + "/busy"), af(sBase + "/engine/" + eng + "/active");
                if (!bf.open(QIODevice::ReadOnly)) continue;
                quint64 bNs = bf.readAll().trimmed().toULongLong(), aNs = af.open(QIODevice::ReadOnly) ? af.readAll().trimmed().toULongLong() : 0;
                QString key = card + "/" + eng;
                if (m_intelPrevBusyNs.contains(key)) { bD += (bNs - m_intelPrevBusyNs[key]); aD += (aNs - m_intelPrevTotalNs[key]); }
                m_intelPrevBusyNs[key] = bNs; m_intelPrevTotalNs[key] = aNs;
            }
            if (aD > 0) gpu["utilization"] = 100.0 * bD / aD;
            gpus.append(gpu);
        }
    }
};
