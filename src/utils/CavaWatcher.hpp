#pragma once
#include <QObject>
#include <QProcess>
#include <QList>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <vector>
#include <algorithm>
#include <QtQml/qqmlregistration.h>

class CavaWatcher : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QList<double> data       READ data        NOTIFY dataChanged)
    Q_PROPERTY(bool          active     READ active      WRITE setActive     NOTIFY activeChanged)
    Q_PROPERTY(int           smoothing  READ smoothing   WRITE setSmoothing  NOTIFY smoothingChanged)
    Q_PROPERTY(int           barCount   READ barCount    CONSTANT)
    Q_PROPERTY(QString       configPath READ configPath  WRITE setConfigPath NOTIFY configPathChanged)

public:
    explicit CavaWatcher(QObject *parent = nullptr) : QObject(parent), m_process(new QProcess(this)) {
        connect(m_process, &QProcess::readyReadStandardOutput, this, &CavaWatcher::onReadyRead);
        connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int, QProcess::ExitStatus) {
            if (!m_active) return;
            m_active = false;
            std::fill(m_data.begin(), m_data.end(), 0.0);
            emit dataChanged();
            emit activeChanged();
        });
        m_data.resize(m_barCount, 0.0);
        m_previousWeights.assign(m_barCount, 0.0);
    }

    ~CavaWatcher() {
        if (m_process->state() != QProcess::NotRunning) {
            m_process->terminate();
            m_process->waitForFinished(500);
        }
    }

    QList<double> data()       const { return m_data; }
    bool          active()     const { return m_active; }
    int           smoothing()  const { return m_smoothing; }
    int           barCount()   const { return m_barCount; }
    QString       configPath() const { return m_configPath; }

    void setSmoothing(int s) {
        if (m_smoothing == s) return;
        m_smoothing = s;
        m_previousWeights.assign(m_barCount, 0.0);
        emit smoothingChanged();
    }

    void setConfigPath(const QString &path) {
        if (m_configPath == path) return;
        m_configPath = path;
        emit configPathChanged();
        if (m_active) { setActive(false); setActive(true); }
    }

    void setActive(bool a) {
        if (m_active == a) return;
        m_active = a;
        if (m_active) {
            QString config = m_configPath.isEmpty() ? QString() : QUrl(m_configPath).toLocalFile();
            if (config.isEmpty()) {
                for (const QString &p : { QDir::homePath() + "/.config/noon/scripts/cava/raw_binary_config.txt",
                                          QDir::homePath() + "/.config/cava/raw_binary_config.txt" }) {
                    if (QFile::exists(p)) { config = p; break; }
                }
            }
            QStringList args;
            if (!config.isEmpty()) args << "-p" << config;
            m_process->start("cava", args);
            if (!m_process->waitForStarted(1000)) {
                m_active = false;
            }
        } else {
            m_process->terminate();
            if (!m_process->waitForFinished(500)) m_process->kill();
            std::fill(m_data.begin(), m_data.end(), 0.0);
            m_previousWeights.assign(m_barCount, 0.0);
            emit dataChanged();
        }
        emit activeChanged();
    }

signals:
    void dataChanged();
    void activeChanged();
    void smoothingChanged();
    void configPathChanged();

private:
    void onReadyRead() {
        if (!m_active) return;
        const QByteArray raw = m_process->readAllStandardOutput();
        const int frameSize = m_barCount * sizeof(uint16_t);
        if (raw.size() < frameSize) return;
        const auto *samples = reinterpret_cast<const uint16_t *>(raw.constData() + raw.size() - frameSize);
        const double sf = 1.0 / (m_smoothing + 1.0), inv = 1.0 - sf;
        for (int i = 0; i < m_barCount; ++i) {
            double val = (samples[i] / 32.0) * sf + m_previousWeights[i] * inv;
            m_previousWeights[i] = m_data[i] = val;
        }
        emit dataChanged();
    }

    static constexpr int m_barCount = 30;
    QProcess            *m_process;
    QList<double>        m_data;
    std::vector<double>  m_previousWeights;
    QString              m_configPath;
    int                  m_smoothing = 1;
    bool                 m_active    = false;
};
