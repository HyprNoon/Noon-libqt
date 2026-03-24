#pragma once
#include <QObject>
#include <QString>
#include <QUrl>
#include <QJsonObject>
#include <QMap>
#include <qqmlintegration.h>
#include <KIO/FileCopyJob>

class DownloadItem : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("DownloadItem is created by DownloadModel")
    Q_PROPERTY(QString label         READ label         CONSTANT)
    Q_PROPERTY(QUrl    url           READ url           CONSTANT)
    Q_PROPERTY(QUrl    destination   READ destination   CONSTANT)
    Q_PROPERTY(int     progress      READ progress      NOTIFY progressChanged)
    Q_PROPERTY(State   state         READ state         NOTIFY stateChanged)
    Q_PROPERTY(qint64  totalBytes    READ totalBytes    NOTIFY totalBytesChanged)
    Q_PROPERTY(qint64  receivedBytes READ receivedBytes NOTIFY receivedBytesChanged)
    Q_PROPERTY(qint64  speed         READ speed         NOTIFY speedChanged)
    Q_PROPERTY(int     eta           READ eta           NOTIFY etaChanged)
public:
    enum class State {
        Queued,
        Running,
        Paused,
        Canceled,
        Finished
    };
    Q_ENUM(State)

    explicit DownloadItem(const QString              &label,
                          const QUrl                 &url,
                          const QUrl                 &destination,
                          const QString              &userAgent = {},
                          const QMap<QString,QString> &headers  = {},
                          QObject                    *parent    = nullptr);

    static DownloadItem *fromJson(const QJsonObject &obj,
                                  const QString     &userAgent = {},
                                  QObject           *parent    = nullptr);

    ~DownloadItem() override = default;

    QString label()         const { return m_label;         }
    QUrl    url()           const { return m_url;           }
    QUrl    destination()   const { return m_destination;   }
    int     progress()      const { return m_progress;      }
    State   state()         const { return m_state;         }
    qint64  totalBytes()    const { return m_totalBytes;    }
    qint64  receivedBytes() const { return m_receivedBytes; }
    qint64  speed()         const { return m_speed;         }
    int     eta()           const { return m_eta;           }

    QJsonObject toJson() const;

    void pause();
    void resume();
    void cancel();
    Q_INVOKABLE void reveal();
    Q_INVOKABLE void open();

signals:
    void progressChanged(int progress);
    void stateChanged(State state);
    void totalBytesChanged(qint64 totalBytes);
    void receivedBytesChanged(qint64 receivedBytes);
    void speedChanged(qint64 speed);
    void etaChanged(int eta);

private slots:
    void onPercent(KJob *job, ulong percent);
    void onResult(KJob *job);
    void onSuspended(KJob *job);
    void onResumed(KJob *job);
    void onTotalSize(KJob *job, KJob::Unit unit, qulonglong amount);
    void onProcessedSize(KJob *job, KJob::Unit unit, qulonglong amount);
    void onSpeedChanged(KJob *job, ulong bytesPerSecond);

private:
    explicit DownloadItem(const QString              &label,
                          const QUrl                 &url,
                          const QUrl                 &destination,
                          int                         restoredProgress,
                          qint64                      restoredTotal,
                          qint64                      restoredReceived,
                          State                       restoredState,
                          const QString              &userAgent,
                          const QMap<QString,QString> &headers,
                          QObject                    *parent);

    void startJob(bool resume);
    void setState(State s);
    void recalcEta();

    static State   stateFromString(const QString &s);
    static QString stateToString(State s);

    QString              m_label;
    QUrl                 m_url;
    QUrl                 m_destination;
    QString              m_userAgent;
    QMap<QString,QString> m_headers;
    int               m_progress      = 0;
    int               m_lastSaved     = 0;
    State             m_state         = State::Queued;
    qint64            m_totalBytes    = 0;
    qint64            m_receivedBytes = 0;
    qint64            m_speed         = 0;
    int               m_eta           = -1;
    KIO::FileCopyJob *m_job           = nullptr;
};
