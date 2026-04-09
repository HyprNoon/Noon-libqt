#pragma once
#include "dw_item.hpp"
#include <QAbstractListModel>
#include <QFileSystemWatcher>
#include <QList>
#include <QUrl>
#include <QMap>
#include <qqmlintegration.h>

class DownloadModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int     count     READ rowCount  NOTIFY countChanged)
    Q_PROPERTY(QString jsonPath  READ jsonPath  WRITE setJsonPath  NOTIFY jsonPathChanged)
    Q_PROPERTY(QString userAgent READ userAgent WRITE setUserAgent NOTIFY userAgentChanged)
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

    explicit DownloadModel(QObject *parent = nullptr);
    ~DownloadModel() override = default;

    int                    rowCount(const QModelIndex &parent = {}) const override;
    QVariant               data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString jsonPath()  const { return m_jsonPath;  }
    QString userAgent() const { return m_userAgent; }

    void setJsonPath(const QString &path);
    void setUserAgent(const QString &userAgent);

    Q_INVOKABLE void add(const QUrl        &url,
                         const QUrl        &destination,
                         const QString     &label   = {},
                         const QVariantMap &headers = {});
    Q_INVOKABLE void remove(int index);
    Q_INVOKABLE void dismiss(int index);
    Q_INVOKABLE void pause(int index);
    Q_INVOKABLE void resume(int index);
    Q_INVOKABLE void cancel(int index);
    Q_INVOKABLE void reveal(int index);
    Q_INVOKABLE void open(int index);
    Q_INVOKABLE int  indexOfUrl(const QUrl &url) const;
    Q_INVOKABLE bool isDownloading(const QUrl &url) const;
    Q_INVOKABLE DownloadItem *get(int index) const;

signals:
    void countChanged();
    void jsonPathChanged();
    void userAgentChanged();
    void downloadFinished(int index, bool success);

private slots:
    void onItemDataChanged();
    void onItemBytesChanged();
    void onItemStateChanged();
    void onFileChanged(const QString &path);

private:
    void load();
    void reload();
    void save();
    bool indexValid(int i)         const;
    int  rowOf(DownloadItem *item) const;
    void connectItem(DownloadItem *item);
    static QMap<QString,QString> toStringMap(const QVariantMap &m);

    QList<DownloadItem *> m_items;
    QString               m_jsonPath;
    QString               m_userAgent;
    QFileSystemWatcher    m_watcher;
    bool                  m_ignoreNextChange = false;
};
