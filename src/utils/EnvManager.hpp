#pragma once
#include <QFile>
#include <QFileSystemWatcher>
#include <QObject>
#include <QQmlPropertyMap>
#include <QRegularExpression>
#include <QTextStream>
#include <QtQml/qqml.h>

class EnvManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(QQmlPropertyMap* data READ data CONSTANT)
    QML_ELEMENT

    static const inline QRegularExpression s_re{R"(^\s*export\s+([A-Za-z_]\w*)\s*=\s*(.*?)\s*$)"};

public:
    explicit EnvManager(QObject *parent = nullptr) : QObject(parent), m_data(QQmlPropertyMap::create(this))
    {
        connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &p) {
            if (!m_watcher.files().contains(p)) m_watcher.addPath(p);
            load();
        });
        connect(m_data, &QQmlPropertyMap::valueChanged, this, [this](const QString &key, const QVariant &val) {
            if (!m_loading) save(key, val.toString());
        });
    }

    QString path() const { return m_path; }
    void setPath(const QString &path) {
        const QString p = path.startsWith("file://") ? QUrl(path).toLocalFile() : path;
        if (m_path != p) { m_path = p; if (!QFile::exists(p)) { QFile f(p); if (!f.open(QIODevice::WriteOnly)) return; f.close(); } m_watcher.addPath(p); load(); emit pathChanged(); } }
    QQmlPropertyMap *data() const { return m_data; }

signals:
    void pathChanged();

private:
    void load()
    {
        QFile f(m_path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
        m_loading = true;
        QTextStream in(&f);
        while (!in.atEnd()) {
            const auto m = s_re.match(in.readLine());
            if (!m.hasMatch()) continue;
            QString key = m.captured(1), val = m.captured(2);
            if (key != key.toUpper()) key = key.toLower();
            if (val.length() > 1 && (val.front() == '"' || val.front() == '\'')) val = val.mid(1, val.length() - 2);
            bool ok; double n = val.toDouble(&ok);
            m_data->insert(key, ok ? QVariant(n) : QVariant(val));
        }
        m_loading = false;
    }

    void save(const QString &changedKey, const QString &changedVal)
    {
        QFile f(m_path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
        QStringList lines; bool found = false;
        QTextStream in(&f);
        while (!in.atEnd()) {
            QString line = in.readLine();
            const auto m = s_re.match(line);
            if (m.hasMatch() && m.captured(1) == changedKey) { line = "export " + changedKey + "=" + changedVal; found = true; }
            lines.append(line);
        }
        f.close();
        if (!found) lines.append("export " + changedKey + "=" + changedVal);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) return;
        QTextStream(&f) << lines.join('\n') << '\n';
    }

    QString m_path;
    QQmlPropertyMap *m_data;
    QFileSystemWatcher m_watcher;
    bool m_loading = false;
};
