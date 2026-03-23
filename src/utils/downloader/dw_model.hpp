#pragma once
#include "dw_item.hpp"
#include <QAbstractListModel>
#include <QList>
#include <QUrl>
#include <qqmlintegration.h>

class DownloadModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int     count    READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString jsonPath READ jsonPath WRITE setJsonPath NOTIFY jsonPathChanged)
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
        EtaRole
    };
    Q_ENUM(Roles)

    explicit DownloadModel(QObject *parent = nullptr);
    ~DownloadModel() override = default;

    int                    rowCount(const QModelIndex &parent = {}) const override;
    QVariant               data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString jsonPath() const { return m_jsonPath; }
    void    setJsonPath(const QString &path);

    Q_INVOKABLE void add(const QUrl    &url,
                         const QUrl    &destination,
                         const QString &label = {});
    Q_INVOKABLE void remove(int index);
    Q_INVOKABLE void dismiss(int index);
    Q_INVOKABLE void pause(int index);
    Q_INVOKABLE void resume(int index);
    Q_INVOKABLE void cancel(int index);
    Q_INVOKABLE int  indexOfUrl(const QUrl &url) const;
    Q_INVOKABLE bool isDownloading(const QUrl &url) const;

signals:
    void countChanged();
    void jsonPathChanged();

private slots:
    void onItemDataChanged();
    void onItemBytesChanged();
    void onItemStateChanged();

private:
    void load();
    void save() const;
    bool indexValid(int i)         const;
    int  rowOf(DownloadItem *item) const;
    void connectItem(DownloadItem *item);

    QList<DownloadItem *> m_items;
    QString               m_jsonPath;
};
