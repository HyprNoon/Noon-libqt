#pragma once
#include <QObject>
#include <QString>
#include <QUrl>
#include <QJsonObject>
#include <QMap>
#include <QFileInfo>
#include <QDesktopServices>
#include <QDateTime>
#include <qqmlintegration.h>
#include <curl/curl.h>

class DownloadModel;

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
    Q_PROPERTY(bool    isNoon        READ isNoon        CONSTANT)
    Q_PROPERTY(qint64  itemMaxSpeed  READ itemMaxSpeed  WRITE setItemMaxSpeed NOTIFY itemMaxSpeedChanged)
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
                          QObject                    *parent    = nullptr)
        : DownloadItem(label, url, destination, 0, 0, 0, State::Queued, userAgent, headers, "noon", parent)
    {}

    static DownloadItem *fromJson(const QJsonObject &obj,
                                   const QString     &userAgent = {},
                                   QObject           *parent    = nullptr)
    {
        const QString label         = obj.value("label").toString();
        const QUrl    url           = QUrl(obj.value("url").toString());
        const QUrl    destination   = QUrl(obj.value("destination").toString());
        const int     progress      = obj.value("progress").toInt(0);
        const qint64  totalBytes    = obj.value("totalBytes").toInteger(0);
        const qint64  receivedBytes = obj.value("receivedBytes").toInteger(0);
        const State   state         = stateFromString(obj.value("state").toString());
        const QString signiture     = obj.value("signiture").toString();
        const qint64  itemMaxSpeed  = obj.value("itemMaxSpeed").toInteger(0);

        QMap<QString,QString> headers;
        const QJsonObject     headersObj = obj.value("headers").toObject();
        for (auto it = headersObj.begin(); it != headersObj.end(); ++it)
            headers.insert(it.key(), it.value().toString());

        if (label.isEmpty() || !url.isValid() || !destination.isValid())
            return nullptr;

        auto *item = new DownloadItem(label, url, destination,
                                       progress, totalBytes, receivedBytes,
                                       state, userAgent, headers, signiture, parent);
        item->m_itemMaxSpeed = itemMaxSpeed;
        return item;
    }

    ~DownloadItem() override
    {
        cleanupTransfer();
    }

    QString label()         const { return m_label;         }
    QUrl    url()           const { return m_url;           }
    QUrl    destination()   const { return m_destination;   }
    int     progress()      const { return m_progress;      }
    State   state()         const { return m_state;         }
    qint64  totalBytes()    const { return m_totalBytes;    }
    qint64  receivedBytes() const { return m_receivedBytes; }
    qint64  speed()         const { return m_speed;         }
    int     eta()           const { return m_eta;           }
    bool    isNoon()        const { return m_signiture == QStringLiteral("noon"); }
    qint64  itemMaxSpeed()  const { return m_itemMaxSpeed; }

    void setItemMaxSpeed(qint64 bytesPerSec)
    {
        if (m_itemMaxSpeed == bytesPerSec)
            return;
        m_itemMaxSpeed = bytesPerSec;
        emit itemMaxSpeedChanged();
        if (m_curl) {
            if (bytesPerSec > 0)
                curl_easy_setopt(m_curl, CURLOPT_MAX_RECV_SPEED_LARGE, bytesPerSec);
            else
                curl_easy_setopt(m_curl, CURLOPT_MAX_RECV_SPEED_LARGE, m_multi ? 0L : 0L);
        }
    }

    QJsonObject toJson() const
    {
        QJsonObject headersObj;
        for (auto it = m_headers.cbegin(); it != m_headers.cend(); ++it)
            headersObj.insert(it.key(), it.value());

        return {
            { "label",         m_label                  },
            { "url",           m_url.toString()         },
            { "destination",   m_destination.toString() },
            { "progress",      m_progress               },
            { "totalBytes",    m_totalBytes             },
            { "receivedBytes", m_receivedBytes          },
            { "state",         stateToString(m_state)   },
            { "headers",       headersObj               },
            { "signiture",     m_signiture              },
            { "itemMaxSpeed",  m_itemMaxSpeed           },
        };
    }

    void pause()
    {
        if (!isNoon() || m_state != State::Running || !m_curl)
            return;
        curl_easy_pause(m_curl, CURLPAUSE_RECV);
        m_speed = 0;
        m_eta   = -1;
        emit speedChanged(m_speed);
        emit etaChanged(m_eta);
        setState(State::Paused);
    }

    void resume()
    {
        if (!isNoon() || m_state != State::Paused || !m_curl)
            return;
        curl_easy_pause(m_curl, CURLPAUSE_CONT);
        setState(State::Running);
    }

    void cancel()
    {
        if (!isNoon() || m_state == State::Canceled)
            return;
        setState(State::Canceled);
        cleanupTransfer();
    }

    Q_INVOKABLE void reveal()
    {
        if (m_destination.isEmpty() || !m_destination.isValid())
            return;

        const QString local = m_destination.toLocalFile();
        if (!QFileInfo::exists(local))
            return;

        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(local).absolutePath()));
    }

    Q_INVOKABLE void open()
    {
        if (m_destination.isEmpty() || !m_destination.isValid())
            return;
        if (isNoon() && m_state != State::Finished)
            return;
        QDesktopServices::openUrl(m_destination);
    }

    void startTransfer(CURLM *multi, qint64 maxSpeed = 0)
    {
        if (!isNoon() || m_state == State::Canceled || m_state == State::Finished)
            return;

        const State initialState = m_state;

        m_curl = curl_easy_init();
        if (!m_curl)
            return;

        const QString localPath = m_destination.toLocalFile();
        m_file = fopen(localPath.toUtf8().constData(), m_receivedBytes > 0 ? "ab" : "wb");
        if (!m_file) {
            curl_easy_cleanup(m_curl);
            m_curl = nullptr;
            setState(State::Canceled);
            return;
        }

        m_lastSpeedTime = QDateTime::currentMSecsSinceEpoch();
        m_lastSpeedBytes = m_receivedBytes;

        curl_easy_setopt(m_curl, CURLOPT_PRIVATE,         this);
        curl_easy_setopt(m_curl, CURLOPT_URL,              m_url.toString().toUtf8().constData());
        curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION,    writeCallback);
        curl_easy_setopt(m_curl, CURLOPT_WRITEDATA,        m_file);
        curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS,       0L);
        curl_easy_setopt(m_curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
        curl_easy_setopt(m_curl, CURLOPT_XFERINFODATA,     this);
        curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION,   1L);
        curl_easy_setopt(m_curl, CURLOPT_FAILONERROR,      1L);
        curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER,   1L);
        curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYHOST,   2L);
        curl_easy_setopt(m_curl, CURLOPT_COOKIEFILE,       "");

        if (m_receivedBytes > 0)
            curl_easy_setopt(m_curl, CURLOPT_RESUME_FROM_LARGE, m_receivedBytes);

        const qint64 effectiveSpeed = m_itemMaxSpeed > 0 ? m_itemMaxSpeed : maxSpeed;
        if (effectiveSpeed > 0)
            curl_easy_setopt(m_curl, CURLOPT_MAX_RECV_SPEED_LARGE, effectiveSpeed);

        if (!m_userAgent.isEmpty())
            curl_easy_setopt(m_curl, CURLOPT_USERAGENT, m_userAgent.toUtf8().constData());

        if (!m_headers.isEmpty()) {
            m_curlHeaders = nullptr;
            for (auto it = m_headers.cbegin(); it != m_headers.cend(); ++it)
                m_curlHeaders = curl_slist_append(m_curlHeaders,
                    (it.key() + ": " + it.value()).toUtf8().constData());
            curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_curlHeaders);
        }

        if (!m_url.host().isEmpty()) {
            const QString origin = m_url.scheme() + "://" + m_url.host();
            if (!m_headers.contains("Referer"))
                curl_easy_setopt(m_curl, CURLOPT_REFERER, origin.toUtf8().constData());
            if (!m_headers.contains("Origin"))
                m_curlHeaders = curl_slist_append(m_curlHeaders,
                    ("Origin: " + origin).toUtf8().constData());
            if (m_curlHeaders)
                curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_curlHeaders);
        }

        curl_multi_add_handle(multi, m_curl);

        m_multi = multi;
        m_state = State::Running;

        if (initialState == State::Paused)
            curl_easy_pause(m_curl, CURLPAUSE_RECV);
    }

    void cleanupTransfer()
    {
        if (m_file) {
            fclose(m_file);
            m_file = nullptr;
        }
        if (m_curlHeaders) {
            curl_slist_free_all(m_curlHeaders);
            m_curlHeaders = nullptr;
        }
        if (m_curl && m_multi) {
            curl_multi_remove_handle(m_multi, m_curl);
            curl_easy_cleanup(m_curl);
            m_curl = nullptr;
            m_multi = nullptr;
        }
    }

    friend class DownloadModel;

signals:
    void progressChanged(int progress);
    void stateChanged(State state);
    void totalBytesChanged(qint64 totalBytes);
    void receivedBytesChanged(qint64 receivedBytes);
    void speedChanged(qint64 speed);
    void etaChanged(int eta);
    void itemMaxSpeedChanged();

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
                          const QString              &signiture,
                          QObject                    *parent)
        : QObject(parent)
        , m_label(label)
        , m_url(url)
        , m_destination(destination)
        , m_userAgent(userAgent)
        , m_headers(headers)
        , m_signiture(signiture)
        , m_progress(restoredProgress)
        , m_state(restoredState)
        , m_totalBytes(restoredTotal)
        , m_receivedBytes(restoredReceived)
    {
    }

    void setState(State s)
    {
        if (m_state == s)
            return;
        m_state = s;
        emit stateChanged(m_state);
    }

    void recalcEta()
    {
        const qint64 remaining = m_totalBytes - m_receivedBytes;
        const int eta = (m_speed > 0 && remaining > 0)
            ? static_cast<int>(remaining / m_speed)
            : -1;

        if (eta == m_eta)
            return;

        m_eta = eta;
        emit etaChanged(m_eta);
    }

    static State stateFromString(const QString &s)
    {
        if (s == "Running")  return State::Running;
        if (s == "Paused")   return State::Paused;
        if (s == "Canceled") return State::Canceled;
        if (s == "Finished") return State::Finished;
        return State::Queued;
    }

    static QString stateToString(State s)
    {
        switch (s) {
        case State::Running:  return "Running";
        case State::Paused:   return "Paused";
        case State::Canceled: return "Canceled";
        case State::Finished: return "Finished";
        default:              return "Queued";
        }
    }

    static size_t writeCallback(void *ptr, size_t size, size_t nmemb, void *userdata)
    {
        auto *file = static_cast<FILE *>(userdata);
        return fwrite(ptr, size, nmemb, file);
    }

    static int progressCallback(void *clientp,
                                 curl_off_t dltotal,
                                 curl_off_t dlnow,
                                 curl_off_t /*ultotal*/,
                                 curl_off_t /*ulnow*/)
    {
        auto *item = static_cast<DownloadItem *>(clientp);

        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        bool changed = false;

        if (dltotal > 0 && dltotal != item->m_totalBytes) {
            item->m_totalBytes = dltotal;
            emit item->totalBytesChanged(item->m_totalBytes);
            changed = true;
        }

        if (dlnow != item->m_receivedBytes) {
            item->m_receivedBytes = dlnow;

            const int p = item->m_totalBytes > 0
                ? static_cast<int>(item->m_receivedBytes * 100 / item->m_totalBytes)
                : 0;
            if (p != item->m_progress) {
                item->m_progress = p;
                emit item->progressChanged(p);
            }
            emit item->receivedBytesChanged(item->m_receivedBytes);

            const qint64 elapsed = now - item->m_lastSpeedTime;
            if (elapsed >= 800) {
                item->m_speed = (item->m_receivedBytes - item->m_lastSpeedBytes) * 1000 / elapsed;
                item->m_lastSpeedTime = now;
                item->m_lastSpeedBytes = item->m_receivedBytes;
                emit item->speedChanged(item->m_speed);
            }

            changed = true;
        }

        if (changed)
            item->recalcEta();

        return 0;
    }

    QString              m_label;
    QUrl                 m_url;
    QUrl                 m_destination;
    QString              m_userAgent;
    QMap<QString,QString> m_headers;
    QString              m_signiture;
    int                  m_progress      = 0;
    State                m_state         = State::Queued;
    qint64               m_totalBytes    = 0;
    qint64               m_receivedBytes = 0;
    qint64               m_speed         = 0;
    qint64               m_itemMaxSpeed  = 0;
    int                  m_eta           = -1;
    qint64               m_lastSpeedTime = 0;
    qint64               m_lastSpeedBytes = 0;

    CURL                *m_curl          = nullptr;
    CURLM               *m_multi         = nullptr;
    FILE                *m_file          = nullptr;
    curl_slist          *m_curlHeaders   = nullptr;
};
