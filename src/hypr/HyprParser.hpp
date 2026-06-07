#pragma once
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantMap>
#include <QQmlPropertyMap>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QFile>
#include <QSaveFile>
#include <QtQml/qqmlregistration.h>
#include <QtConcurrent/QtConcurrent>

class HyprParser : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(QQmlPropertyMap* variables READ variables CONSTANT)
    Q_PROPERTY(bool isLoaded READ isLoaded NOTIFY isLoadedChanged)
public:
    explicit HyprParser(QObject *parent = nullptr) : QObject(parent) {
        m_vars      = QQmlPropertyMap::create(this);
        m_watcher   = new QFileSystemWatcher(this);
        m_saveTimer = new QTimer(this);
        m_saveTimer->setSingleShot(true);
        m_saveTimer->setInterval(300);
        connect(m_vars,      &QQmlPropertyMap::valueChanged,   this, [this](const QString &) { if (!m_internalUpdating) m_saveTimer->start(); });
        connect(m_watcher,   &QFileSystemWatcher::fileChanged, this, &HyprParser::reload);
        connect(m_saveTimer, &QTimer::timeout,                 this, &HyprParser::executeSave);
    }

    QString path() const { return m_path; }

    void setPath(const QString &path) {
        QString local = path.startsWith("file://") ? QUrl(path).toLocalFile() : path;
        if (m_path == local) return;
        if (!m_path.isEmpty()) m_watcher->removePath(m_path);
        m_path = local;
        m_watcher->addPath(m_path);
        reload();
        emit pathChanged();
    }

    QQmlPropertyMap* variables() const { return m_vars; }
    bool isLoaded() const { return m_isLoaded; }

    Q_INVOKABLE void reload() {
        if (m_path.isEmpty()) return;
        if (!m_watcher->files().contains(m_path)) m_watcher->addPath(m_path);
        QtConcurrent::run([path = m_path]() { return parse(path); })
            .then(this, [this](const QVariantMap &result) {
                m_internalUpdating = true;
                for (auto it = result.begin(); it != result.end(); ++it)
                    m_vars->insert(it.key(), it.value());
                m_internalUpdating = false;
                if (!m_isLoaded) { m_isLoaded = true; emit isLoadedChanged(); }
            });
    }

    Q_INVOKABLE void forceSave() { m_saveTimer->stop(); executeSave(); }

signals:
    void pathChanged();
    void isLoadedChanged();

private:
    QString m_path;
    QQmlPropertyMap *m_vars;
    QFileSystemWatcher *m_watcher;
    QTimer *m_saveTimer;
    bool m_isLoaded = false;
    bool m_internalUpdating = false;

    void executeSave() {
        if (m_path.isEmpty() || m_internalUpdating) return;
        const QString path = m_path;
        QVariantMap data;
        for (const QString &k : m_vars->keys()) data[k] = m_vars->value(k);

        (void)QtConcurrent::run([path, data]() {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
            const QList<QByteArray> raw = file.readAll().split('\n');
            file.close();

            QByteArrayList out;
            for (const QByteArray &rawLine : raw) {
                QString t = QString::fromUtf8(rawLine).trimmed();
                if (!t.isEmpty() && !t.startsWith("--")) {
                    int eq = t.indexOf('=');
                    if (eq > 0) {
                        QString key = t.left(eq).trimmed();
                        QString vraw = t.mid(eq + 1);
                        int ci = vraw.indexOf("--");
                        QString comment = ci != -1 ? "  " + vraw.mid(ci) : QString();
                        QStringView v = QStringView(ci != -1 ? vraw.left(ci) : vraw).trimmed();
                        if (data.contains(key) && isLiteral(v)) {
                            out << QString("%1 = %2%3").arg(key, formatValue(data[key]), comment).toUtf8();
                            continue;
                        }
                    }
                }
                out << rawLine;
            }

            QSaveFile sf(path);
            if (sf.open(QIODevice::WriteOnly | QIODevice::Text)) {
                sf.write(out.join('\n'));
                sf.write("\n");
                sf.commit();
            }
        });
    }

    static QVariantMap parse(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
        const QList<QByteArray> lines = file.readAll().split('\n');
        file.close();
        QVariantMap map;
        for (const QByteArray &raw : lines) {
            QString line = QString::fromUtf8(raw).trimmed();
            if (line.isEmpty() || line.startsWith("--")) continue;
            int ci = line.indexOf("--");
            if (ci != -1) line = line.left(ci).trimmed();
            int eq = line.indexOf('=');
            if (eq <= 0) continue;
            QString key     = line.left(eq).trimmed();
            QStringView val = QStringView(line).mid(eq + 1).trimmed();
            if (key.contains(' ') || key.contains('.') || key.contains('[')) continue;
            map.insert(key, parseValue(val));
        }
        return map;
    }

    static QVariant parseValue(QStringView s) {
        if (s.startsWith('"') && s.endsWith('"') && s.size() >= 2) return s.mid(1, s.size() - 2).toString();
        if (s == u"true")  return true;
        if (s == u"false") return false;
        bool ok = false;
        if (qlonglong i = s.toLongLong(&ok); ok) return i;
        if (double d    = s.toDouble(&ok);   ok) return d;
        return s.toString();
    }

    static QString formatValue(const QVariant &v) {
        switch (v.userType()) {
        case QMetaType::Bool:     return v.toBool() ? "true" : "false";
        case QMetaType::LongLong:
        case QMetaType::Int:
        case QMetaType::Double:
        case QMetaType::Float:    return v.toString();
        default: break;
        }
        QString s = v.toString();
        if (s == "true" || s == "false") return s;
        bool num = false; s.toDouble(&num); if (num) return s;
        return s.startsWith('"') ? s : QStringLiteral("\"%1\"").arg(s);
    }

    static bool isLiteral(QStringView v) {
        if (v.isEmpty()) return false;
        if (v.startsWith('"') && v.endsWith('"')) return true;
        if (v == u"true" || v == u"false") return true;
        bool ok = false;
        if (double d    = v.toDouble(&ok);   ok) { Q_UNUSED(d) return true; }
        if (qlonglong i = v.toLongLong(&ok); ok) { Q_UNUSED(i) return true; }
        for (QChar c : v) if (c == '(' || c == ')' || c == '.' || c == ' ') return false;
        return true;
    }
};
