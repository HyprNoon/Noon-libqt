#pragma once
#include <QObject>
#include <QtQml/qqmlregistration.h>
#include <QtQml/QQmlParserStatus>
#include <QDirIterator>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QDir>
#include <QUrl>

class FileSystemWatcher : public QObject, public QQmlParserStatus {
    Q_OBJECT
    QML_ELEMENT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QString folder READ folder WRITE setFolder NOTIFY folderChanged REQUIRED)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QStringList excludeFilters READ excludeFilters WRITE setExcludeFilters NOTIFY excludeFiltersChanged)

public:
    explicit FileSystemWatcher(QObject *parent = nullptr)
        : QObject(parent), m_enabled(true), m_ready(false), m_initialized(false), m_stateSum(0) {
        m_debounce.setSingleShot(true);
        m_debounce.setInterval(50);
        connect(&m_debounce,   &QTimer::timeout,                       this,        &FileSystemWatcher::scan);
        connect(&m_sysWatcher, &QFileSystemWatcher::directoryChanged,  &m_debounce, qOverload<>(&QTimer::start));
        connect(&m_sysWatcher, &QFileSystemWatcher::fileChanged,       &m_debounce, qOverload<>(&QTimer::start));
    }

    void classBegin() override {}
    void componentComplete() override { m_ready = true; scan(); }

    QString folder() const { return m_path; }
    void setFolder(const QString &path) {
        QString resolved = path;
        if (resolved.startsWith("file://")) resolved = QUrl(resolved).toLocalFile();
        if (resolved.startsWith("~"))       resolved.replace(0, 1, QDir::homePath());
        if (m_path == resolved) return;
        m_path = resolved;
        m_initialized = false;
        emit folderChanged();
        if (m_ready) m_debounce.start();
    }

    bool enabled() const { return m_enabled; }
    void setEnabled(bool value) {
        if (m_enabled == value) return;
        m_enabled = value;
        emit enabledChanged();
        if (m_ready) m_debounce.start();
    }

    QStringList excludeFilters() const { return m_excludeFilters; }
    void setExcludeFilters(const QStringList &filters) {
        if (m_excludeFilters == filters) return;
        m_excludeFilters = filters;
        m_initialized = false;
        emit excludeFiltersChanged();
        if (m_ready) m_debounce.start();
    }

signals:
    void folderChanged();
    void enabledChanged();
    void excludeFiltersChanged();
    void contentsChanged(const QString &path);

private:
    void scan() {
        if (!m_ready) return;

        if (!m_enabled || m_path.isEmpty()) {
            if (!m_watched.isEmpty()) {
                m_sysWatcher.removePaths(m_watched.values());
                m_watched.clear();
            }
            m_stateSum    = 0;
            m_initialized = false;
            return;
        }

        QSet<QString> found;
        found << m_path;
        qint64 sum = 0;

        QDirIterator it(m_path, m_excludeFilters, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QFileInfo info(it.next());
            sum ^= (sum << 5) + info.size() + info.lastModified().toMSecsSinceEpoch();
            found << info.absoluteFilePath();
        }

        const QSet<QString> toRemove = m_watched - found;
        const QSet<QString> toAdd    = found - m_watched;
        if (!toRemove.isEmpty()) m_sysWatcher.removePaths(toRemove.values());
        if (!toAdd.isEmpty())    m_sysWatcher.addPaths(toAdd.values());
        m_watched = found;

        if (m_initialized && sum != m_stateSum)
            emit contentsChanged(m_path);

        m_stateSum    = sum;
        m_initialized = true;
    }

    bool               m_enabled;
    bool               m_ready;
    bool               m_initialized;
    qint64             m_stateSum;
    QString            m_path;
    QStringList        m_excludeFilters;
    QSet<QString>      m_watched;
    QFileSystemWatcher m_sysWatcher;
    QTimer             m_debounce;
};
