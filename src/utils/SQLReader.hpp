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
#include <QFileInfo>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QMutex>
#include <atomic>
#include <qqml.h>

static QMutex s_dbMtx;
static std::atomic<int> s_connId{0};

struct DbGuard {
    QSqlDatabase db;
    QString conn;
    DbGuard(const QString &path)
        : conn(QStringLiteral("db%1").arg(s_connId.fetch_add(1, std::memory_order_relaxed))) {
        QMutexLocker l(&s_dbMtx);
        db = QSqlDatabase::addDatabase("QSQLITE", conn);
        db.setDatabaseName(path);
        db.setConnectOptions("QSQLITE_OPEN_READONLY");
    }
    ~DbGuard() {
        db.close();
        db = QSqlDatabase(); // release handle before removeDatabase
        QMutexLocker l(&s_dbMtx);
        QSqlDatabase::removeDatabase(conn);
    }
};

static inline QString qid(const QString &s) {
    QString r = s; r.replace('"', "\"\"");
    return '"' + r + '"';
}

static inline QVariantList runSql(const QString &path, const QString &sql,
                                   const QVariantList &bind = {}) {
    QVariantList rows;
    DbGuard g(path);
    if (g.db.open()) {
        QSqlQuery q(g.db);
        q.prepare(sql);
        for (const auto &b : bind) q.addBindValue(b);
        if (q.exec()) {
            const auto rec = q.record();
            for (int n = rec.count(); q.next(); ) {
                QVariantMap row;
                for (int i = 0; i < n; ++i) row.insert(rec.fieldName(i), q.value(i));
                rows << row;
            }
        }
    }
    return rows;
}

class SqlTable : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QStringList columns READ columns CONSTANT)
    QML_UNCREATABLE("")
public:
    SqlTable(const QString &name, const QString &path, const QStringList &cols, QObject *parent)
        : QObject(parent), m_name(name), m_path(path), m_columns(cols) {}
    QString name() const { return m_name; }
    QStringList columns() const { return m_columns; }
    Q_INVOKABLE void queryAll() { run("SELECT * FROM " + qid(m_name)); }
    Q_INVOKABLE void queryWhere(const QVariantMap &cond) {
        if (cond.isEmpty()) return queryAll();
        QStringList clauses; QVariantList bind;
        for (auto it = cond.constBegin(); it != cond.constEnd(); ++it) {
            clauses << qid(it.key()) + " = ?"; bind << it.value();
        }
        run("SELECT * FROM " + qid(m_name) + " WHERE " + clauses.join(" AND "), bind);
    }
    Q_INVOKABLE void queryFind(const QString &col, const QVariant &val) {
        run("SELECT * FROM " + qid(m_name) + " WHERE " + qid(col) + " = ? LIMIT 1", {val});
    }
signals:
    void results(QVariantList rows);
private:
    void run(const QString &sql, const QVariantList &bind = {}) {
        const QString path = m_path;
        auto *w = new QFutureWatcher<QVariantList>(this);
        connect(w, &QFutureWatcher<QVariantList>::finished, this, [this, w] {
            emit results(w->result()); w->deleteLater();
        });
        w->setFuture(QtConcurrent::run([path, sql, bind] { return runSql(path, sql, bind); }));
    }
    QString m_name, m_path;
    QStringList m_columns;
};

class SQLReader : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(double loadedAt READ loadedAt NOTIFY loadedAtChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QQmlPropertyMap* tables READ tables CONSTANT)
    QML_ELEMENT
    struct OpenResult { QString error; QStringList tables; QMap<QString,QStringList> columns; double ts = 0; };
public:
    explicit SQLReader(QObject *parent = nullptr) : QObject(parent), m_tables(QQmlPropertyMap::create(this)) {}
    ~SQLReader() { purge(); }
    QString path() const { return m_path; }
    bool loading() const { return m_loading; }
    double loadedAt() const { return m_loadedAt; }
    QString error() const { return m_error; }
    QQmlPropertyMap *tables() const { return m_tables; }
    void setPath(const QString &p) {
        const QString abs = QFileInfo(QUrl(p).isLocalFile() ? QUrl(p).toLocalFile() : p).absoluteFilePath();
        if (m_path == abs) return;
        m_path = abs; emit pathChanged(); open();
    }
    Q_INVOKABLE void reload() { if (!m_path.isEmpty() && !m_loading) open(); }
    Q_INVOKABLE void query(const QString &sql, const QVariantList &bind = {}) {
        const QString path = m_path;
        auto *w = new QFutureWatcher<QVariantList>(this);
        connect(w, &QFutureWatcher<QVariantList>::finished, this, [this, w] {
            emit queryFinished(w->result()); w->deleteLater();
        });
        w->setFuture(QtConcurrent::run([path, sql, bind] { return runSql(path, sql, bind); }));
    }
signals:
    void pathChanged();
    void loadingChanged();
    void loadedAtChanged();
    void errorChanged();
    void loaded();
    void queryFinished(QVariantList rows);
private:
    void open() {
        purge(); ++m_gen;
        set(m_loading, true, &SQLReader::loadingChanged);
        set(m_error, {}, &SQLReader::errorChanged);
        set(m_loadedAt, 0.0, &SQLReader::loadedAtChanged);
        const auto path = m_path;
        const int gen = m_gen;
        auto *w = new QFutureWatcher<OpenResult>(this);
        connect(w, &QFutureWatcher<OpenResult>::finished, this, [this, gen, w] {
            const auto r = w->result(); w->deleteLater();
            if (gen != m_gen) return;
            set(m_loading, false, &SQLReader::loadingChanged);
            if (!r.error.isEmpty()) { set(m_error, r.error, &SQLReader::errorChanged); return; }
            for (const auto &t : r.tables) {
                auto *tbl = new SqlTable(t, m_path, r.columns.value(t), this);
                m_owned << tbl;
                m_tables->insert(t, QVariant::fromValue(tbl));
            }
            set(m_loadedAt, r.ts, &SQLReader::loadedAtChanged);
            emit loaded();
        });
        w->setFuture(QtConcurrent::run([path]() -> OpenResult {
            OpenResult r;
            DbGuard g(path);
            if (g.db.open()) {
                r.tables = g.db.tables();
                for (const auto &t : r.tables) {
                    QSqlQuery q(g.db);
                    if (q.exec("PRAGMA table_info(" + qid(t) + ")"))
                        while (q.next()) r.columns[t] << q.value("name").toString();
                }
            } else r.error = g.db.lastError().text();
            r.ts = static_cast<double>(QDateTime::currentMSecsSinceEpoch());
            return r;
        }));
    }
    void purge() {
        for (const auto &k : m_tables->keys()) m_tables->clear(k);
        qDeleteAll(m_owned); m_owned.clear();
    }
    template<typename T, typename Signal>
    void set(T &field, const T &val, Signal sig) { if (field != val) { field = val; emit (this->*sig)(); } }
    QString m_path;
    bool m_loading = false;
    double m_loadedAt = 0.0;
    QString m_error;
    QQmlPropertyMap *m_tables;
    QList<SqlTable *> m_owned;
    int m_gen = 0;
};
