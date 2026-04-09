#include "dw_model.hpp"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QUrl>

static QString localPath(const QString &path)
{
    const QUrl url(path);
    return url.isLocalFile() ? url.toLocalFile() : path;
}

DownloadModel::DownloadModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(&m_watcher, &QFileSystemWatcher::fileChanged,
            this, &DownloadModel::onFileChanged);
}

void DownloadModel::setJsonPath(const QString &path)
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

void DownloadModel::setUserAgent(const QString &userAgent)
{
    if (m_userAgent == userAgent)
        return;
    m_userAgent = userAgent;
    emit userAgentChanged();
}

void DownloadModel::load()
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
    }

    if (!m_items.isEmpty())
        emit countChanged();
}


void DownloadModel::reload()
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

    // Signal that the entire model is changing
    beginResetModel();

    // Clean up existing items
    qDeleteAll(m_items);
    m_items.clear();

    // Rebuild from JSON
    for (const QJsonValue &val : arr) {
        if (!val.isObject())
            continue;

        DownloadItem *item = DownloadItem::fromJson(val.toObject(), m_userAgent, this);
        if (item) {
            connectItem(item);
            m_items.append(item);
        }
    }

    endResetModel();

    emit countChanged();
    m_watcher.addPath(localPath(m_jsonPath));
}

void DownloadModel::onFileChanged(const QString &path)
{
    Q_UNUSED(path)
    if (m_ignoreNextChange) {
        m_ignoreNextChange = false;
        m_watcher.addPath(localPath(m_jsonPath));
        return;
    }
    reload();
}

void DownloadModel::save()
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

int DownloadModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_items.size();
}

QVariant DownloadModel::data(const QModelIndex &index, int role) const
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

QHash<int, QByteArray> DownloadModel::roleNames() const
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

QMap<QString,QString> DownloadModel::toStringMap(const QVariantMap &m)
{
    QMap<QString,QString> result;
    for (auto it = m.cbegin(); it != m.cend(); ++it)
        result.insert(it.key(), it.value().toString());
    return result;
}

void DownloadModel::add(const QUrl &url, const QUrl &destination, const QString &label, const QVariantMap &headers)
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

    emit countChanged();
    save();
}

void DownloadModel::remove(int index)
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
}

void DownloadModel::dismiss(int index)
{
    if (!indexValid(index) || !m_items.at(index)->isNoon())
        return;

    beginRemoveRows({}, index, index);
    DownloadItem *item = m_items.takeAt(index);
    endRemoveRows();

    item->deleteLater();

    emit countChanged();
    save();
}

void DownloadModel::pause(int index)
{
    if (indexValid(index) && m_items.at(index)->isNoon())
        m_items.at(index)->pause();
}

void DownloadModel::resume(int index)
{
    if (indexValid(index) && m_items.at(index)->isNoon())
        m_items.at(index)->resume();
}

void DownloadModel::cancel(int index)
{
    if (indexValid(index) && m_items.at(index)->isNoon())
        m_items.at(index)->cancel();
}

void DownloadModel::open(int index)
{
    if (indexValid(index))
        m_items.at(index)->open();
}

void DownloadModel::reveal(int index)
{
    if (indexValid(index))
        m_items.at(index)->reveal();
}

int DownloadModel::indexOfUrl(const QUrl &url) const
{
    for (int i = 0; i < m_items.size(); ++i)
        if (m_items.at(i)->url() == url)
            return i;
    return -1;
}

bool DownloadModel::isDownloading(const QUrl &url) const
{
    return indexOfUrl(url) != -1;
}

void DownloadModel::onItemDataChanged()
{
    const int row = rowOf(qobject_cast<DownloadItem *>(sender()));
    if (row < 0)
        return;

    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, { ProgressRole });

    if (m_items.at(row)->progress() % 10 == 0)
        save();
}

void DownloadModel::onItemBytesChanged()
{
    const int row = rowOf(qobject_cast<DownloadItem *>(sender()));
    if (row < 0)
        return;

    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, { TotalBytesRole, ReceivedBytesRole, SpeedRole, EtaRole });
}

void DownloadModel::onItemStateChanged()
{
    const int row = rowOf(qobject_cast<DownloadItem *>(sender()));
    if (row < 0)
        return;

    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, { StateRole });

    DownloadItem *item = m_items.at(row);
    if (item->state() == DownloadItem::State::Finished) {
        emit downloadFinished(row, true);
    } else if (item->state() == DownloadItem::State::Canceled) {
        emit downloadFinished(row, false);
    }

    save();
}

DownloadItem *DownloadModel::get(int index) const
{
    return indexValid(index) ? m_items.at(index) : nullptr;
}

bool DownloadModel::indexValid(int i) const
{
    return i >= 0 && i < m_items.size();
}

int DownloadModel::rowOf(DownloadItem *item) const
{
    return m_items.indexOf(item);
}

void DownloadModel::connectItem(DownloadItem *item)
{
    connect(item, &DownloadItem::progressChanged,      this, &DownloadModel::onItemDataChanged);
    connect(item, &DownloadItem::stateChanged,         this, &DownloadModel::onItemStateChanged);
    connect(item, &DownloadItem::totalBytesChanged,    this, &DownloadModel::onItemBytesChanged);
    connect(item, &DownloadItem::receivedBytesChanged, this, &DownloadModel::onItemBytesChanged);
    connect(item, &DownloadItem::speedChanged,         this, &DownloadModel::onItemBytesChanged);
    connect(item, &DownloadItem::etaChanged,           this, &DownloadModel::onItemBytesChanged);
}
