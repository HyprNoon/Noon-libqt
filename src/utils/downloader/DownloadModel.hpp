#pragma once
#include "DownloadItem.hpp"
#include <QAbstractListModel>
#include <QFileSystemWatcher>
#include <QList>
#include <QUrl>
#include <QMap>
#include <QTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <qqmlintegration.h>

class DownloadModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int     count      READ rowCount   NOTIFY countChanged)
    Q_PROPERTY(QString jsonPath   READ jsonPath   WRITE setJsonPath   NOTIFY jsonPathChanged)
    Q_PROPERTY(QString userAgent  READ userAgent  WRITE setUserAgent  NOTIFY userAgentChanged)
    Q_PROPERTY(qint64  maxSpeed   READ maxSpeed   WRITE setMaxSpeed   NOTIFY maxSpeedChanged)
    Q_PROPERTY(int     maxParallel READ maxParallel WRITE setMaxParallel NOTIFY maxParallelChanged)
public:
    enum Roles {
        LabelRole         = Qt::UserRole + 1,
        UrlRole,
        DestinationRole,
        ProgressRole,
        StateRole,
        ItemRole,
        TotalBytesRole,
        ReceivedBytesRole,
        SpeedRole,
        EtaRole,
        IsNoonRole,
    };
    Q_ENUM(Roles)

    explicit DownloadModel(QObject *parent = nullptr)
        : QAbstractListModel(parent)
    {
        static bool curlInited = false;
        if (!curlInited) {
            curl_global_init(CURL_GLOBAL_ALL);
            curlInited = true;
        }
        m_multi = curl_multi_init();

        m_multiTimer = new QTimer(this);
        connect(m_multiTimer, &QTimer::timeout, this, &DownloadModel::onTick);
        m_multiTimer->start(100);

        connect(&m_watcher, &QFileSystemWatcher::fileChanged,
                this, &DownloadModel::onFileChanged);
    }

    ~DownloadModel() override
    {
        m_multiTimer->stop();
        if (m_multi) {
            curl_multi_cleanup(m_multi);
            m_multi = nullptr;
        }
    }

    int rowCount(const QModelIndex &parent = {}) const override
    {
        if (parent.isValid())
            return 0;
        return m_items.size();
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.row() >= m_items.size())
            return {};

        const DownloadItem *item = m_items.at(index.row());

        switch (role) {
        case LabelRole:          return item->label();
        case UrlRole:            return item->url();
        case DestinationRole:    return item->destination();
        case ProgressRole:       return item->progress();
        case StateRole:          return QVariant::fromValue(item->state());
        case ItemRole:           return QVariant::fromValue(item);
        case TotalBytesRole:     return item->totalBytes();
        case ReceivedBytesRole:  return item->receivedBytes();
        case SpeedRole:          return item->speed();
        case EtaRole:            return item->eta();
        case IsNoonRole:         return item->isNoon();
        default:                 return {};
        }
    }

    QHash<int, QByteArray> roleNames() const override
    {
        return {
            { LabelRole,         "label"         },
            { UrlRole,           "url"           },
            { DestinationRole,   "destination"   },
            { ProgressRole,      "progress"      },
            { StateRole,         "state"         },
            { ItemRole,          "item"          },
            { TotalBytesRole,    "totalBytes"    },
            { ReceivedBytesRole, "receivedBytes" },
            { SpeedRole,         "speed"         },
            { EtaRole,           "eta"           },
            { IsNoonRole,        "isNoon"        },
        };
    }

    QString jsonPath()     const { return m_jsonPath;  }
    QString userAgent()    const { return m_userAgent; }
    qint64  maxSpeed()     const { return m_maxSpeed;  }
    int     maxParallel()  const { return m_maxParallel; }

    void setJsonPath(const QString &path)
    {
        if (m_jsonPath == path)
            return;

        if (!m_jsonPath.isEmpty())
            m_watcher.removePath(localPath(m_jsonPath));

        m_jsonPath = path;
        emit jsonPathChanged();

        const QString local = localPath(m_jsonPath);
        if (QFileInfo::exists(local))
            m_watcher.addPath(local);

        load();
    }

    void setUserAgent(const QString &userAgent)
    {
        if (m_userAgent == userAgent)
            return;
        m_userAgent = userAgent;
        emit userAgentChanged();
    }

    void setMaxSpeed(qint64 bytesPerSec)
    {
        if (m_maxSpeed == bytesPerSec)
            return;
        m_maxSpeed = bytesPerSec;
        emit maxSpeedChanged();

        for (auto *item : m_items) {
            if (item->m_curl && item->m_itemMaxSpeed == 0)
                curl_easy_setopt(item->m_curl, CURLOPT_MAX_RECV_SPEED_LARGE, m_maxSpeed);
        }
    }

    void setMaxParallel(int n)
    {
        if (m_maxParallel == n)
            return;
        m_maxParallel = n;
        emit maxParallelChanged();

        startNextQueued();
    }

    Q_INVOKABLE void add(const QUrl        &url,
                         const QUrl        &destination,
                         const QString     &label   = {},
                         const QVariantMap &headers = {})
    {
        const QUrl resolvedDest = destination.scheme().isEmpty()
            ? QUrl::fromLocalFile(destination.path())
            : destination;

        const QString resolvedLabel = label.isEmpty()
            ? QFileInfo(url.path()).fileName()
            : label;

        auto *item = new DownloadItem(resolvedLabel, url, resolvedDest, m_userAgent, toStringMap(headers), this);
        connectItem(item);

        const int row = m_items.size();
        beginInsertRows({}, row, row);
        m_items.append(item);
        endInsertRows();

        if (m_maxParallel == 0 || runningCount() < m_maxParallel)
            item->startTransfer(m_multi, m_maxSpeed);
        else
            item->setState(DownloadItem::State::Queued);

        emit countChanged();
        save();
    }

    Q_INVOKABLE void remove(int index)
    {
        if (!indexValid(index) || !m_items.at(index)->isNoon())
            return;

        beginRemoveRows({}, index, index);
        DownloadItem *item = m_items.takeAt(index);
        endRemoveRows();

        item->cancel();
        item->deleteLater();

        emit countChanged();
        save();
        startNextQueued();
    }

    Q_INVOKABLE void dismiss(int index)
    {
        if (!indexValid(index) || !m_items.at(index)->isNoon())
            return;

        beginRemoveRows({}, index, index);
        DownloadItem *item = m_items.takeAt(index);
        endRemoveRows();

        item->deleteLater();

        emit countChanged();
        save();
        startNextQueued();
    }

    Q_INVOKABLE void pause(int index)
    {
        if (indexValid(index) && m_items.at(index)->isNoon())
            m_items.at(index)->pause();
    }

    Q_INVOKABLE void resume(int index)
    {
        if (indexValid(index) && m_items.at(index)->isNoon())
            m_items.at(index)->resume();
    }

    Q_INVOKABLE void cancel(int index)
    {
        if (indexValid(index) && m_items.at(index)->isNoon())
            m_items.at(index)->cancel();
    }

    Q_INVOKABLE void open(int index)
    {
        if (indexValid(index))
            m_items.at(index)->open();
    }

    Q_INVOKABLE void reveal(int index)
    {
        if (indexValid(index))
            m_items.at(index)->reveal();
    }

    Q_INVOKABLE void clearAll()
    {
        for (int i = m_items.size() - 1; i >= 0; --i) {
            DownloadItem *item = m_items.at(i);
            if (!item->isNoon())
                continue;

            if (item->state() == DownloadItem::State::Finished
                || item->state() == DownloadItem::State::Canceled)
                dismiss(i);
            else
                remove(i);
        }
    }

    Q_INVOKABLE int indexOfUrl(const QUrl &url) const
    {
        for (int i = 0; i < m_items.size(); ++i)
            if (m_items.at(i)->url() == url)
                return i;
        return -1;
    }

    Q_INVOKABLE void retry(int index)
    {
        if (!indexValid(index))
            return;

        DownloadItem *item = m_items.at(index);
        const QUrl url       = item->url();
        const QUrl dest      = item->destination();
        const QString label  = item->label();

        remove(index);
        add(url, dest, label);
    }

    Q_INVOKABLE bool isDownloading(const QUrl &url) const
    {
        return indexOfUrl(url) != -1;
    }

    Q_INVOKABLE DownloadItem *get(int index) const
    {
        return indexValid(index) ? m_items.at(index) : nullptr;
    }

signals:
    void countChanged();
    void jsonPathChanged();
    void userAgentChanged();
    void maxSpeedChanged();
    void maxParallelChanged();
    void downloadFinished(int index, bool success);

private slots:
    void onTick()
    {
        int running;
        curl_multi_perform(m_multi, &running);

        CURLMsg *msg;
        int msgs;
        while ((msg = curl_multi_info_read(m_multi, &msgs))) {
            if (msg->msg != CURLMSG_DONE)
                continue;

            DownloadItem *item = nullptr;
            curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &item);
            if (!item)
                continue;

            item->cleanupTransfer();

            if (item->state() == DownloadItem::State::Canceled)
                continue;

            if (msg->data.result == CURLE_OK)
                item->setState(DownloadItem::State::Finished);
            else
                item->setState(DownloadItem::State::Canceled);
        }

        startNextQueued();
    }

    void onItemDataChanged()
    {
        const int row = rowOf(qobject_cast<DownloadItem *>(sender()));
        if (row < 0)
            return;

        const QModelIndex idx = index(row);
        emit dataChanged(idx, idx, { ProgressRole });

        if (m_items.at(row)->progress() % 10 == 0)
            save();
    }

    void onItemBytesChanged()
    {
        const int row = rowOf(qobject_cast<DownloadItem *>(sender()));
        if (row < 0)
            return;

        const QModelIndex idx = index(row);
        emit dataChanged(idx, idx, { TotalBytesRole, ReceivedBytesRole, SpeedRole, EtaRole });
    }

    void onItemStateChanged()
    {
        const int row = rowOf(qobject_cast<DownloadItem *>(sender()));
        if (row < 0)
            return;

        const QModelIndex idx = index(row);
        emit dataChanged(idx, idx, { StateRole });

        DownloadItem *item = m_items.at(row);
        if (item->state() == DownloadItem::State::Finished)
            emit downloadFinished(row, true);
        else if (item->state() == DownloadItem::State::Canceled)
            emit downloadFinished(row, false);

        save();
    }

    void onFileChanged(const QString &path)
    {
        Q_UNUSED(path)
        if (m_ignoreNextChange) {
            m_ignoreNextChange = false;
            m_watcher.addPath(localPath(m_jsonPath));
            return;
        }
        reload();
    }

private:
    static QString localPath(const QString &path)
    {
        const QUrl url(path);
        return url.isLocalFile() ? url.toLocalFile() : path;
    }

    void load()
    {
        if (m_jsonPath.isEmpty())
            return;

        QFile file(localPath(m_jsonPath));
        if (!file.exists() || !file.open(QIODevice::ReadOnly))
            return;

        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();

        if (!doc.isObject())
            return;

        const QJsonArray arr = doc.object().value("downloads").toArray();
        for (const QJsonValue &val : arr) {
            if (!val.isObject())
                continue;

            DownloadItem *item = DownloadItem::fromJson(val.toObject(), m_userAgent, this);
            if (!item)
                continue;

            connectItem(item);
            const int row = m_items.size();
            beginInsertRows({}, row, row);
            m_items.append(item);
            endInsertRows();

            item->startTransfer(m_multi, m_maxSpeed);
        }

        if (!m_items.isEmpty())
            emit countChanged();
    }

    void reload()
    {
        if (m_jsonPath.isEmpty())
            return;

        QFile file(localPath(m_jsonPath));
        if (!file.exists() || !file.open(QIODevice::ReadOnly))
            return;

        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();

        if (!doc.isObject())
            return;

        const QJsonArray arr = doc.object().value("downloads").toArray();

        beginResetModel();

        qDeleteAll(m_items);
        m_items.clear();

        for (const QJsonValue &val : arr) {
            if (!val.isObject())
                continue;

            DownloadItem *item = DownloadItem::fromJson(val.toObject(), m_userAgent, this);
            if (item) {
                connectItem(item);
                m_items.append(item);
                item->startTransfer(m_multi, m_maxSpeed);
            }
        }

        endResetModel();

        emit countChanged();
        m_watcher.addPath(localPath(m_jsonPath));
    }

    void save()
    {
        if (m_jsonPath.isEmpty())
            return;

        const QString local = localPath(m_jsonPath);
        QDir().mkpath(QFileInfo(local).absolutePath());

        QJsonArray arr;
        for (const DownloadItem *item : m_items)
            arr.append(item->toJson());

        QJsonDocument doc(QJsonObject{{ "downloads", arr }});

        QFile file(local);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            m_ignoreNextChange = true;
            file.write(doc.toJson(QJsonDocument::Indented));
        }
    }

    bool indexValid(int i) const
    {
        return i >= 0 && i < m_items.size();
    }

    int rowOf(DownloadItem *item) const
    {
        return m_items.indexOf(item);
    }

    void connectItem(DownloadItem *item)
    {
        connect(item, &DownloadItem::progressChanged,      this, &DownloadModel::onItemDataChanged);
        connect(item, &DownloadItem::stateChanged,         this, &DownloadModel::onItemStateChanged);
        connect(item, &DownloadItem::totalBytesChanged,    this, &DownloadModel::onItemBytesChanged);
        connect(item, &DownloadItem::receivedBytesChanged, this, &DownloadModel::onItemBytesChanged);
        connect(item, &DownloadItem::speedChanged,         this, &DownloadModel::onItemBytesChanged);
        connect(item, &DownloadItem::etaChanged,           this, &DownloadModel::onItemBytesChanged);
        connect(item, &DownloadItem::itemMaxSpeedChanged,  this, &DownloadModel::onItemBytesChanged);
    }

    int runningCount() const
    {
        if (m_maxParallel == 0)
            return 0;

        int n = 0;
        for (const auto *item : m_items) {
            if (item->state() == DownloadItem::State::Running)
                ++n;
        }
        return n;
    }

    void startNextQueued()
    {
        if (m_maxParallel == 0)
            return;

        for (auto *item : m_items) {
            if (item->state() == DownloadItem::State::Queued
                && runningCount() < m_maxParallel)
                item->startTransfer(m_multi, m_maxSpeed);
        }
    }

    static QMap<QString,QString> toStringMap(const QVariantMap &m)
    {
        QMap<QString,QString> result;
        for (auto it = m.cbegin(); it != m.cend(); ++it)
            result.insert(it.key(), it.value().toString());
        return result;
    }

    QList<DownloadItem *> m_items;
    QString               m_jsonPath;
    QString               m_userAgent;
    QFileSystemWatcher    m_watcher;
    bool                  m_ignoreNextChange = false;
    CURLM                *m_multi            = nullptr;
    QTimer               *m_multiTimer       = nullptr;
    qint64                m_maxSpeed         = 0;
    int                   m_maxParallel      = 0;
};
