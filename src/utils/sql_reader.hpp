#pragma once

#include <QObject>
#include <QQmlPropertyMap>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QVariantList>
#include <QVariantMap>
#include <QStringList>
#include <QUrl>
#include <QDateTime>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <qqml.h>

class SqlTable : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList columns READ columns CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    QML_UNCREATABLE("SqlTable is created internally by SQLReader")

public:
    explicit SqlTable(const QString &table_name, const QString &connection_name, QObject *parent = nullptr)
        : QObject(parent), m_name(table_name), m_connection_name(connection_name)
    {
        QSqlQuery q(QSqlDatabase::database(m_connection_name, false));
        q.exec(QString("PRAGMA table_info(%1)").arg(m_name));
        while (q.next()) m_columns << q.value("name").toString();
    }

    QString name() const { return m_name; }
    QStringList columns() const { return m_columns; }

    Q_INVOKABLE QVariantList all() const {
        return exec(QString("SELECT * FROM %1").arg(m_name));
    }

    Q_INVOKABLE QVariantList where(const QVariantMap &conditions) const {
        if (conditions.isEmpty()) return all();
        QVariantList bindings;
        QStringList clauses;
        for (auto it = conditions.constBegin(); it != conditions.constEnd(); ++it) {
            clauses << QString("%1 = ?").arg(it.key());
            bindings << it.value();
        }
        return exec(QString("SELECT * FROM %1 WHERE %2").arg(m_name, clauses.join(" AND ")), bindings);
    }

    Q_INVOKABLE QVariant find(const QString &column, const QVariant &value) const {
        const auto r = exec(QString("SELECT * FROM %1 WHERE %2 = ? LIMIT 1").arg(m_name, column), { value });
        return r.isEmpty() ? QVariant() : r.first();
    }

private:
    QString m_name;
    QString m_connection_name;
    QStringList m_columns;

    QVariantList exec(const QString &sql, const QVariantList &bindings = {}) const {
        QVariantList results;
        auto db = QSqlDatabase::database(m_connection_name, false);
        if (!db.isOpen()) return results;
        QSqlQuery q(db);
        q.prepare(sql);
        for (const auto &b : bindings) q.addBindValue(b);
        if (!q.exec()) { qWarning() << "SqlTable:" << m_name << q.lastError().text(); return results; }
        const auto rec = q.record();
        const int n = rec.count();
        while (q.next()) {
            QVariantMap row;
            for (int i = 0; i < n; ++i) row.insert(rec.fieldName(i), q.value(i));
            results << row;
        }
        return results;
    }
};

class SQLReader : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString path READ path WRITE set_path NOTIFY path_changed)
    Q_PROPERTY(double loaded_at READ loaded_at NOTIFY loaded_at_changed)
    Q_PROPERTY(QString error READ error NOTIFY error_changed)
    Q_PROPERTY(QQmlPropertyMap* tables READ tables CONSTANT)
    QML_ELEMENT

public:
    explicit SQLReader(QObject *parent = nullptr)
        : QObject(parent), m_tables(QQmlPropertyMap::create(this))
    {
        m_connection_name = QString("sqlreader_%1").arg(++s_instance_count);
    }

    ~SQLReader() { close(); }

    QString path() const { return m_path; }
    void set_path(const QString &path) {
        const QString p = QUrl(path).isLocalFile() ? QUrl(path).toLocalFile() : path;
        if (m_path == p) return;
        m_path = p;
        emit path_changed();
        open();
    }

    double loaded_at() const { return m_loaded_at; }
    QString error() const { return m_error; }
    QQmlPropertyMap *tables() const { return m_tables; }

    Q_INVOKABLE void reload() { if (!m_path.isEmpty()) { close(); open(); } }

    Q_INVOKABLE void queryAsync(const QString &sql, const QVariantList &bindings = {}) {
        const QString path = m_path;
        const QString connBase = m_connection_name;
        const QString asyncConn = connBase + "_async_" + QString::number(++m_async_counter);

        auto *watcher = new QFutureWatcher<QVariantList>(this);

        connect(watcher, &QFutureWatcher<QVariantList>::finished, this, [this, watcher, asyncConn]() {
            QSqlDatabase::removeDatabase(asyncConn);
            emit queryFinished(watcher->result());
            watcher->deleteLater();
        });

        auto future = QtConcurrent::run([path, asyncConn, sql, bindings]() -> QVariantList {
            QVariantList results;
            {
                auto db = QSqlDatabase::addDatabase("QSQLITE", asyncConn);
                db.setDatabaseName(path);
                db.setConnectOptions("QSQLITE_OPEN_READONLY");
                if (!db.open()) return results;

                QSqlQuery q(db);
                q.prepare(sql);
                for (const auto &b : bindings) q.addBindValue(b);
                if (!q.exec()) { qWarning() << "SQLReader async:" << q.lastError().text(); return results; }

                const auto rec = q.record();
                const int n = rec.count();
                while (q.next()) {
                    QVariantMap row;
                    for (int i = 0; i < n; ++i) row.insert(rec.fieldName(i), q.value(i));
                    results << row;
                }
                db.close();
            }
            return results;
        });

        watcher->setFuture(future);
    }

signals:
    void path_changed();
    void loaded_at_changed();
    void error_changed();
    void loaded();
    void queryFinished(QVariantList results);

private:
    void open() {
        set_loaded_at(0);
        set_error("");
        if (QSqlDatabase::contains(m_connection_name)) QSqlDatabase::removeDatabase(m_connection_name);
        auto db = QSqlDatabase::addDatabase("QSQLITE", m_connection_name);
        db.setDatabaseName(m_path);
        db.setConnectOptions("QSQLITE_OPEN_READONLY");
        if (!db.open()) { set_error(db.lastError().text()); return; }
        for (const auto &t : db.tables())
            m_tables->insert(t, QVariant::fromValue(new SqlTable(t, m_connection_name, this)));
        set_loaded_at(static_cast<double>(QDateTime::currentMSecsSinceEpoch()));
        emit loaded();
    }

    void close() {
        for (const auto &k : m_tables->keys()) m_tables->clear(k);
        if (QSqlDatabase::contains(m_connection_name)) {
            QSqlDatabase::database(m_connection_name).close();
            QSqlDatabase::removeDatabase(m_connection_name);
        }
        set_loaded_at(0);
    }

    void set_error(const QString &msg) { if (m_error != msg) { m_error = msg; emit error_changed(); } }
    void set_loaded_at(double ts) { if (m_loaded_at != ts) { m_loaded_at = ts; emit loaded_at_changed(); } }

    QString m_path;
    double m_loaded_at = 0;
    QString m_error;
    QString m_connection_name;
    QQmlPropertyMap *m_tables;
    int m_async_counter = 0;

    static inline int s_instance_count = 0;
};
