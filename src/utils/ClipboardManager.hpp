#pragma once
#include <QAbstractListModel>
#include <QClipboard>
#include <QGuiApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStringList>
#include <QImage>
#include <QMimeData>
#include <QTimer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QVariantList>
#include <QVariantMap>
#include <QQmlEngine>
#include <qqml.h>
#include <QtConcurrent/QtConcurrentRun>

class ClipboardManager : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int limit MEMBER m_limit NOTIFY limitChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)

public:
    enum Roles {
        TextRole = Qt::UserRole + 1,
        TypeRole,
        ImagePathRole,
        TimestampRole
    };
    Q_ENUM(Roles)

    explicit ClipboardManager(QObject* parent = nullptr)
        : QAbstractListModel(parent)
        , m_clipboard(QGuiApplication::clipboard())
        , m_reloadTimer(new QTimer(this))
    {
        m_reloadTimer->setSingleShot(true);
        m_reloadTimer->setInterval(50);
        connect(m_reloadTimer, &QTimer::timeout, this, &ClipboardManager::performScheduledReload);

        QTimer::singleShot(0, this, [this]() {
            if (m_enabled && !m_initialized)
                init();
        });
    }

    ~ClipboardManager() {
        if (m_db.isOpen())
            m_db.close();
    }

    bool enabled() const { return m_enabled; }

    void setEnabled(bool enabled) {
        if (m_enabled == enabled) {
            if (enabled && !m_initialized) init();
            return;
        }
        m_enabled = enabled;
        if (enabled && !m_initialized) init();
        emit enabledChanged();
    }

    int rowCount(const QModelIndex& parent = {}) const override {
        if (parent.isValid()) return 0;
        return m_fullEntries.size();
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() >= m_fullEntries.size())
            return {};
        const Entry& entry = m_fullEntries[index.row()];
        switch (role) {
        case TextRole: {
            if (entry.type == "image")
                return "Image";
            QString text = entry.content;
            text.replace('\n', ' ').replace('\t', ' ');
            if (text.length() > 100)
                text = text.left(100) + "...";
            return text;
        }
        case TypeRole:
            return entry.type;
        case ImagePathRole:
            return entry.imagePath;
        case TimestampRole:
            return QDateTime::fromSecsSinceEpoch(entry.timestamp);
        default:
            return {};
        }
    }

    QHash<int, QByteArray> roleNames() const override {
        return {
            {TextRole, "text"},
            {TypeRole, "type"},
            {ImagePathRole, "imagePath"},
            {TimestampRole, "timestamp"}
        };
    }

    QVariantList entries() const {
        return m_cachedEntries;
    }

    Q_INVOKABLE void copyByIndex(int index) {
        if (index < 0 || index >= m_fullEntries.size()) return;
        const Entry& entry = m_fullEntries[index];

        if (entry.type == "image") {
            ++m_pendingOps;
            QFuture<void> future = QtConcurrent::run([this, path = entry.imagePath]() {
                QImage image(path);
                QMetaObject::invokeMethod(this, [this, image]() {
                    --m_pendingOps;
                    if (m_pendingOps > 0) return;
                    m_ignoreNextChange = true;
                    if (!image.isNull())
                        m_clipboard->setImage(image);
                    else
                        m_ignoreNextChange = false;
                }, Qt::QueuedConnection);
            });
            Q_UNUSED(future);
        } else {
            m_ignoreNextChange = true;
            m_clipboard->setText(entry.content);
        }
    }

    Q_INVOKABLE void copy(const QString& text) {
        m_ignoreNextChange = true;
        m_clipboard->setText(text, QClipboard::Clipboard);
    }

    Q_INVOKABLE void deleteEntry(int index) {
        if (index < 0 || index >= m_fullEntries.size()) return;
        const Entry& entry = m_fullEntries[index];

        if (entry.type == "image" && !entry.imagePath.isEmpty()) {
            QSqlQuery refQuery(m_db);
            refQuery.prepare("SELECT COUNT(*) FROM clipboard_history WHERE image_path = :path AND id != :id");
            refQuery.bindValue(":path", entry.imagePath);
            refQuery.bindValue(":id", entry.id);
            if (refQuery.exec() && refQuery.next() && refQuery.value(0).toInt() == 0)
                QFile::remove(entry.imagePath);
        }

        QSqlQuery query(m_db);
        query.prepare("DELETE FROM clipboard_history WHERE id = :id");
        query.bindValue(":id", entry.id);
        if (query.exec()) scheduleReload();
    }

    Q_INVOKABLE void wipe() {
        QString imagePath = QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation) + "/clipboard_images/";
        QDir dir(imagePath);
        dir.removeRecursively();

        QSqlQuery query(m_db);
        if (!query.exec("DELETE FROM clipboard_history"))
            return;

        query.exec("DELETE FROM sqlite_sequence WHERE name='clipboard_history'");
        query.exec("VACUUM");
        scheduleReload();
    }

signals:
    void entriesRefreshed();
    void entriesChanged();
    void enabledChanged();
    void limitChanged();

private:
    void init() {
        initDatabase();
        loadHistory();
        connect(m_clipboard, &QClipboard::dataChanged, this, &ClipboardManager::onClipboardChanged);
        m_initialized = true;
    }

    void initDatabase() {
        QString dbPath = QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation) + "/clipboard.db";
        QDir().mkpath(QFileInfo(dbPath).path());

        m_db = QSqlDatabase::addDatabase("QSQLITE", "clipboard_connection");
        m_db.setDatabaseName(dbPath);

        if (!m_db.open()) return;

        QSqlQuery query(m_db);
        query.exec(R"(
            CREATE TABLE IF NOT EXISTS clipboard_history (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                type TEXT NOT NULL,
                content TEXT,
                content_hash TEXT,
                image_path TEXT,
                timestamp INTEGER NOT NULL
            )
        )");

        QSqlQuery checkColumn(m_db);
        checkColumn.exec("PRAGMA table_info(clipboard_history)");
        bool hasHashColumn = false;
        while (checkColumn.next()) {
            if (checkColumn.value(1).toString() == "content_hash") {
                hasHashColumn = true;
                break;
            }
        }
        if (!hasHashColumn)
            query.exec("ALTER TABLE clipboard_history ADD COLUMN content_hash TEXT");

        query.exec("CREATE INDEX IF NOT EXISTS idx_timestamp ON clipboard_history(timestamp DESC)");
        query.exec("CREATE INDEX IF NOT EXISTS idx_hash ON clipboard_history(content_hash)");
    }

    void loadHistory() {
        if (m_isProcessing) return;
        m_isProcessing = true;

        beginResetModel();
        m_fullEntries.clear();

        QSqlQuery query(m_db);
        query.prepare("SELECT * FROM clipboard_history ORDER BY timestamp DESC LIMIT :limit");
        query.bindValue(":limit", m_limit);

        if (!query.exec()) {
            endResetModel();
            m_isProcessing = false;
            return;
        }

        while (query.next()) {
            Entry entry;
            entry.id = query.value("id").toInt();
            entry.type = query.value("type").toString();
            entry.content = query.value("content").toString();
            entry.imagePath = query.value("image_path").toString();
            entry.timestamp = query.value("timestamp").toLongLong();
            m_fullEntries.append(entry);
        }

        endResetModel();

        m_cachedEntries.clear();
        for (int i = 0; i < m_fullEntries.size(); i++) {
            const Entry& e = m_fullEntries[i];
            QVariantMap item;
            item["index"] = i;
            item["imagePath"] = e.imagePath;
            if (e.type == "image") {
                item["text"] = "Image";
                item["isImage"] = true;
            } else {
                QString text = e.content;
                text.replace('\n', ' ').replace('\t', ' ');
                if (text.length() > 100)
                    text = text.left(100) + "...";
                item["text"] = text;
                item["isImage"] = false;
            }
            m_cachedEntries.append(item);
        }

        QSqlQuery cleanupQuery(m_db);
        cleanupQuery.prepare(R"(
            DELETE FROM clipboard_history
            WHERE id NOT IN (
                SELECT id FROM clipboard_history
                ORDER BY timestamp DESC
                LIMIT :limit
            )
        )");
        cleanupQuery.bindValue(":limit", m_limit);
        cleanupQuery.exec();

        m_isProcessing = false;
        emit entriesChanged();
        emit entriesRefreshed();
    }

    void scheduleReload() {
        if (!m_reloadTimer->isActive())
            m_reloadTimer->start();
    }

    void storeText(const QString& text) {
        QByteArray hash = QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256).toHex();

        QSqlQuery checkQuery(m_db);
        checkQuery.prepare("SELECT id FROM clipboard_history WHERE content_hash = :hash AND type = 'text' LIMIT 1");
        checkQuery.bindValue(":hash", QString::fromLatin1(hash));

        if (checkQuery.exec() && checkQuery.next()) {
            int existingId = checkQuery.value(0).toInt();
            QSqlQuery updateQuery(m_db);
            updateQuery.prepare("UPDATE clipboard_history SET timestamp = :timestamp WHERE id = :id");
            updateQuery.bindValue(":timestamp", QDateTime::currentSecsSinceEpoch());
            updateQuery.bindValue(":id", existingId);
            if (updateQuery.exec()) scheduleReload();
            return;
        }

        QSqlQuery query(m_db);
        query.prepare(R"(
            INSERT INTO clipboard_history (type, content, content_hash, timestamp)
            VALUES ('text', :content, :hash, :timestamp)
        )");
        query.bindValue(":content", text);
        query.bindValue(":hash", QString::fromLatin1(hash));
        query.bindValue(":timestamp", QDateTime::currentSecsSinceEpoch());
        if (query.exec()) scheduleReload();
    }

    void storeImage(const QImage& image) {
        QImage normalized = image.convertToFormat(QImage::Format_ARGB32);
        QByteArray hash = QCryptographicHash::hash(
            QByteArray(reinterpret_cast<const char*>(normalized.constBits()), normalized.sizeInBytes()),
            QCryptographicHash::Sha256
        ).toHex();

        QSqlQuery checkQuery(m_db);
        checkQuery.prepare("SELECT id FROM clipboard_history WHERE content_hash = :hash AND type = 'image' LIMIT 1");
        checkQuery.bindValue(":hash", QString::fromLatin1(hash));

        if (checkQuery.exec() && checkQuery.next()) {
            int existingId = checkQuery.value(0).toInt();
            QSqlQuery updateQuery(m_db);
            updateQuery.prepare("UPDATE clipboard_history SET timestamp = :timestamp WHERE id = :id");
            updateQuery.bindValue(":timestamp", QDateTime::currentSecsSinceEpoch());
            updateQuery.bindValue(":id", existingId);
            if (updateQuery.exec()) scheduleReload();
            return;
        }

        QString imageDir = QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation) + "/clipboard_images/";
        QDir().mkpath(imageDir);

        QString fullPath = imageDir + QString::fromLatin1(hash) + ".png";
        if (!QFile::exists(fullPath))
            if (!image.save(fullPath)) return;

        QSqlQuery query(m_db);
        query.prepare(R"(
            INSERT INTO clipboard_history (type, content_hash, image_path, timestamp)
            VALUES ('image', :hash, :path, :timestamp)
        )");
        query.bindValue(":hash", QString::fromLatin1(hash));
        query.bindValue(":path", fullPath);
        query.bindValue(":timestamp", QDateTime::currentSecsSinceEpoch());
        if (query.exec()) scheduleReload();
    }

private slots:
    void onClipboardChanged() {
        if (m_ignoreNextChange) {
            m_ignoreNextChange = false;
            return;
        }
        if (m_isProcessing) return;

        const QMimeData* mimeData = m_clipboard->mimeData();
        if (!mimeData) return;

        if (mimeData->hasImage()) {
            QImage image = qvariant_cast<QImage>(mimeData->imageData());
            if (!image.isNull()) storeImage(image);
        } else if (mimeData->hasText()) {
            QString text = mimeData->text();
            if (!text.isEmpty()) storeText(text);
        }
    }

    void performScheduledReload() {
        loadHistory();
    }

private:
    QClipboard* m_clipboard;
    QSqlDatabase m_db;
    int m_limit = 300;
    bool m_enabled = true;
    bool m_ignoreNextChange = false;
    QTimer* m_reloadTimer;
    bool m_isProcessing = false;
    int m_pendingOps = 0;
    bool m_initialized = false;

    struct Entry {
        int id;
        QString type;
        QString content;
        QString imagePath;
        qint64 timestamp;
    };

    QList<Entry> m_fullEntries;
    QVariantList m_cachedEntries;
};
