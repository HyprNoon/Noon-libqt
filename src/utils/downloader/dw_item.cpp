#include "dw_item.hpp"

#include <KIO/OpenUrlJob>
#include <KIO/OpenFileManagerWindowJob>
#include <QFileInfo>
#include <KIO/Job>

DownloadItem::DownloadItem(const QString              &label,
                           const QUrl                 &url,
                           const QUrl                 &destination,
                           const QString              &userAgent,
                           const QMap<QString,QString> &headers,
                           QObject                    *parent)
    : DownloadItem(label, url, destination, 0, 0, 0, State::Queued, userAgent, headers, parent)
{}

DownloadItem::DownloadItem(const QString              &label,
                           const QUrl                 &url,
                           const QUrl                 &destination,
                           int                         restoredProgress,
                           qint64                      restoredTotal,
                           qint64                      restoredReceived,
                           State                       restoredState,
                           const QString              &userAgent,
                           const QMap<QString,QString> &headers,
                           QObject                    *parent)
    : QObject(parent)
    , m_label(label)
    , m_url(url)
    , m_destination(destination)
    , m_userAgent(userAgent)
    , m_headers(headers)
    , m_progress(restoredProgress)
    , m_state(restoredState)
    , m_totalBytes(restoredTotal)
    , m_receivedBytes(restoredReceived)
{
    if (m_state == State::Canceled || m_state == State::Finished)
        return;

    const bool hasPartial = QFileInfo(m_destination.toLocalFile()).size() > 0;
    startJob(hasPartial);

    if (restoredState == State::Paused)
        m_job->suspend();
}

DownloadItem *DownloadItem::fromJson(const QJsonObject &obj,
                                     const QString     &userAgent,
                                     QObject           *parent)
{
    const QString label         = obj.value("label").toString();
    const QUrl    url           = QUrl(obj.value("url").toString());
    const QUrl    destination   = QUrl(obj.value("destination").toString());
    const int     progress      = obj.value("progress").toInt(0);
    const qint64  totalBytes    = obj.value("totalBytes").toInteger(0);
    const qint64  receivedBytes = obj.value("receivedBytes").toInteger(0);
    const State   state         = stateFromString(obj.value("state").toString());

    QMap<QString,QString> headers;
    const QJsonObject     headersObj = obj.value("headers").toObject();
    for (auto it = headersObj.begin(); it != headersObj.end(); ++it)
        headers.insert(it.key(), it.value().toString());

    if (label.isEmpty() || !url.isValid() || !destination.isValid())
        return nullptr;

    return new DownloadItem(label, url, destination,
                            progress, totalBytes, receivedBytes,
                            state, userAgent, headers, parent);
}

void DownloadItem::startJob(bool resume)
{
    const KIO::JobFlags flags = resume
        ? (KIO::Resume    | KIO::HideProgressInfo)
        : (KIO::Overwrite | KIO::HideProgressInfo);

    m_job = KIO::file_copy(m_url, m_destination, -1, flags);

    if (!m_userAgent.isEmpty())
        m_job->addMetaData("UserAgent", m_userAgent);

    m_job->addMetaData("SendLanguageSettings", "false");
    m_job->addMetaData("cookies",              "none");
    m_job->addMetaData("cache",                "reload");
    m_job->addMetaData("no-auth",              "true");

    if (!m_url.host().isEmpty()) {
        const QString origin = m_url.scheme() + "://" + m_url.host();
        if (!m_headers.contains("Referer"))
            m_job->addMetaData("Referer", origin);
        if (!m_headers.contains("Origin"))
            m_job->addMetaData("Origin", origin);
    }

    for (auto it = m_headers.cbegin(); it != m_headers.cend(); ++it)
        m_job->addMetaData(it.key(), it.value());

    connect(m_job, &KJob::percentChanged,         this, &DownloadItem::onPercent);
    connect(m_job, &KJob::result,                 this, &DownloadItem::onResult);
    connect(m_job, &KJob::suspended,              this, &DownloadItem::onSuspended);
    connect(m_job, &KJob::resumed,                this, &DownloadItem::onResumed);
    connect(m_job, &KJob::totalAmountChanged,     this, &DownloadItem::onTotalSize);
    connect(m_job, &KJob::processedAmountChanged, this, &DownloadItem::onProcessedSize);
    connect(m_job, &KJob::speed,                  this, &DownloadItem::onSpeedChanged);

    m_state = State::Running;
    m_job->start();
}

QJsonObject DownloadItem::toJson() const
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
    };
}

DownloadItem::State DownloadItem::stateFromString(const QString &s)
{
    if (s == "Running")  return State::Running;
    if (s == "Paused")   return State::Paused;
    if (s == "Canceled") return State::Canceled;
    if (s == "Finished") return State::Finished;
    return State::Queued;
}

QString DownloadItem::stateToString(State s)
{
    switch (s) {
    case State::Running:  return "Running";
    case State::Paused:   return "Paused";
    case State::Canceled: return "Canceled";
    case State::Finished: return "Finished";
    default:              return "Queued";
    }
}

void DownloadItem::pause()
{
    if (m_state != State::Running || !m_job)
        return;
    m_job->suspend();
}

void DownloadItem::resume()
{
    if (m_state != State::Paused || !m_job)
        return;
    m_job->resume();
}

void DownloadItem::cancel()
{
    if (m_state == State::Canceled || !m_job)
        return;
    setState(State::Canceled);
    m_job->kill(KJob::EmitResult);
}

void DownloadItem::reveal()
{
    if (m_destination.isEmpty() || !m_destination.isValid())
        return;
    KIO::highlightInFileManager({m_destination});
}

void DownloadItem::open()
{
    if (m_state != State::Finished)
        return;
    if (m_destination.isEmpty() || !m_destination.isValid())
        return;
    auto *job = new KIO::OpenUrlJob(m_destination, this);
    job->start();
}

void DownloadItem::onPercent(KJob * /*job*/, ulong percent)
{
    const int p = static_cast<int>(percent);
    if (p == m_progress)
        return;
    m_progress = p;
    emit progressChanged(m_progress);
}

void DownloadItem::onResult(KJob *job)
{
    m_job = nullptr;

    if (m_state == State::Canceled)
        return;

    if (job->error()) {
        setState(State::Canceled);
    } else {
        setState(State::Finished);
    }
}

void DownloadItem::onSuspended(KJob * /*job*/)
{
    m_speed = 0;
    m_eta   = -1;
    emit speedChanged(m_speed);
    emit etaChanged(m_eta);
    setState(State::Paused);
}

void DownloadItem::onResumed(KJob * /*job*/)
{
    setState(State::Running);
}

void DownloadItem::onTotalSize(KJob * /*job*/, KJob::Unit unit, qulonglong amount)
{
    if (unit != KJob::Unit::Bytes)
        return;

    const qint64 v = static_cast<qint64>(amount);
    if (v == m_totalBytes)
        return;

    m_totalBytes = v;
    emit totalBytesChanged(m_totalBytes);
    recalcEta();
}

void DownloadItem::onProcessedSize(KJob * /*job*/, KJob::Unit unit, qulonglong amount)
{
    if (unit != KJob::Unit::Bytes)
        return;

    const qint64 v = static_cast<qint64>(amount);
    if (v == m_receivedBytes)
        return;

    m_receivedBytes = v;
    emit receivedBytesChanged(m_receivedBytes);
    recalcEta();
}

void DownloadItem::onSpeedChanged(KJob * /*job*/, ulong bytesPerSecond)
{
    const qint64 s = static_cast<qint64>(bytesPerSecond);
    if (s == m_speed)
        return;

    m_speed = s;
    emit speedChanged(m_speed);
    recalcEta();
}

void DownloadItem::recalcEta()
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

void DownloadItem::setState(State s)
{
    if (m_state == s)
        return;
    m_state = s;
    emit stateChanged(m_state);
}
