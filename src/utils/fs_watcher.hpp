#pragma once
#include <QObject>
#include <QQmlEngine>
#include <QtQml/qqmlregistration.h>
#include <QDirIterator>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QDir>
#include <QUrl>

class FileSystemWatcher : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString folder READ folder WRITE setFolder NOTIFY folderChanged REQUIRED)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

public:
    explicit FileSystemWatcher(QObject *parent = nullptr)
        : QObject(parent), m_enabled(true), m_currentStateSum(0) {
        connect(&m_sysWatcher, &QFileSystemWatcher::directoryChanged, this, &FileSystemWatcher::updateState);
        connect(&m_sysWatcher, &QFileSystemWatcher::fileChanged, this, &FileSystemWatcher::updateState);
    }

    QString folder() const { return m_folder; }
    void setFolder(const QString &path) {
        if (m_folder != path) {
            m_folder = path;
            emit folderChanged();

            m_resolvedPath = m_folder;
            if (m_resolvedPath.startsWith("file://")) m_resolvedPath = QUrl(m_resolvedPath).toLocalFile();
            if (m_resolvedPath.startsWith("~")) m_resolvedPath.replace(0, 1, QDir::homePath());

            updateState();
        }
    }

    bool enabled() const { return m_enabled; }
    void setEnabled(bool value) {
        if (m_enabled != value) {
            m_enabled = value;
            emit enabledChanged();
            updateState();
        }
    }

signals:
    void folderChanged();
    void enabledChanged();
    void fileChanged(const QString &path);

private:
    void updateState() {
        auto tracked = m_sysWatcher.files() + m_sysWatcher.directories();
        if (!tracked.isEmpty()) m_sysWatcher.removePaths(tracked);

        if (!m_enabled || m_resolvedPath.isEmpty()) {
            m_currentStateSum = 0;
            return;
        }

        QQmlEngine *engine = qmlEngine(this);
        QStringList pathsToWatch;
        pathsToWatch << m_resolvedPath;

        qint64 newStateSum = 0;
        QDirIterator it(m_resolvedPath, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::NoIteratorFlags);

        while (it.hasNext()) {
            QString currentPath = it.next();
            QFileInfo info(currentPath);
            QString absolutePath = info.absoluteFilePath();

            if (info.fileName() == "qmldir" && engine) {
                QString folderPath = info.absolutePath();
                if (!engine->importPathList().contains(folderPath)) {
                    engine->addImportPath(folderPath);
                }
            }

            newStateSum += info.size() + info.lastModified().toMSecsSinceEpoch();
            pathsToWatch << absolutePath;
        }

        m_sysWatcher.addPaths(pathsToWatch);

        if (m_currentStateSum != 0 && newStateSum != m_currentStateSum) {
            emit fileChanged(m_resolvedPath);
        }
        m_currentStateSum = newStateSum;
    }

    bool m_enabled;
    QString m_folder;
    QString m_resolvedPath;
    qint64 m_currentStateSum;
    QFileSystemWatcher m_sysWatcher;
};
