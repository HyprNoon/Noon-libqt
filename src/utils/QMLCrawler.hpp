#pragma once
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFuture>
#include <QFutureWatcher>
#include <QObject>
#include <QQmlEngine>
#include <QSet>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent/QtConcurrent>
#include <QtQml/QQmlParserStatus>
#include <QtQml/qqmlregistration.h>

class QmlCrawler : public QObject, public QQmlParserStatus {
    Q_OBJECT
    QML_ELEMENT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QString folder READ folder WRITE setFolder NOTIFY folderChanged REQUIRED)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

    struct ScanResult {
        QSet<QString> files;
        size_t        hash = 0;
    };

public:
    explicit QmlCrawler(QObject *parent = nullptr) : QObject(parent) {
        m_debounce.setSingleShot(true);
        m_debounce.setInterval(50);
        connect(&m_debounce,   &QTimer::timeout,                 this, &QmlCrawler::onDebounce);
        connect(&m_sysWatcher, &QFileSystemWatcher::fileChanged, this, &QmlCrawler::onFileChanged);
        connect(&m_watcher,    &QFutureWatcherBase::finished,    this, &QmlCrawler::onScanFinished);
    }

    void classBegin() override {}
    void componentComplete() override { m_ready = true; scheduleFullScan(); }

    QString folder() const { return m_path; }
    void setFolder(const QString &path) {
        QString r = path;
        if (r.startsWith("file://")) r = QUrl(r).toLocalFile();
        if (r.startsWith("~"))       r.replace(0, 1, QDir::homePath());
        if (m_path == r) return;
        m_path        = r;
        m_initialized = false;
        emit folderChanged();
        scheduleFullScan();
    }

    bool enabled() const { return m_enabled; }
    void setEnabled(bool v) {
        if (m_enabled == v) return;
        m_enabled = v;
        emit enabledChanged();
        scheduleFullScan();
    }

signals:
    void folderChanged();
    void enabledChanged();
    void contentsChanged(const QString &path);

private:
    void scheduleFullScan() { if (m_ready) { m_pendingFull = true; m_debounce.start(); } }

    void onFileChanged(const QString &file) {
        if (QFileInfo(file).fileName() == "qmldir") m_pendingFull = true;
        m_debounce.start();
    }

    void onDebounce() {
        if (m_pendingFull) {
            m_pendingFull = false;
            if (m_watcher.isRunning()) { m_pendingFull = true; return; } // retry when finished
            if (!m_enabled || m_path.isEmpty()) { applyResult({}); return; }
            const QString path = m_path;
            m_watcher.setFuture(QtConcurrent::run([path]() { return crawl(path); }));
        } else {
            quickSum();
        }
    }

    void onScanFinished() {
        applyResult(m_watcher.result());
        if (m_pendingFull) { m_pendingFull = false; onDebounce(); } // coalesced rescan
    }

    void applyResult(const ScanResult &result) {
        if (QQmlEngine *engine = qmlEngine(this)) {
            for (const QString &f : result.files) {
                if (QFileInfo(f).fileName() == "qmldir") {
                    const QString ip = QFileInfo(f).absolutePath();
                    if (!engine->importPathList().contains(ip)) engine->addImportPath(ip);
                }
            }
        }
        const QSet<QString> toRemove = m_watched - result.files;
        const QSet<QString> toAdd    = result.files - m_watched;
        if (!toRemove.isEmpty()) m_sysWatcher.removePaths(toRemove.values());
        if (!toAdd.isEmpty())    m_sysWatcher.addPaths(toAdd.values());
        m_watched = result.files;
        emitIfChanged(result.hash);
    }

    void emitIfChanged(size_t hash) {
        if (m_initialized && hash != m_stateHash) emit contentsChanged(m_path);
        m_stateHash   = hash;
        m_initialized = true;
    }

    void quickSum() {
        if (!m_initialized) return;
        size_t hash = 0;
        for (const QString &p : std::as_const(m_watched)) {
            const QFileInfo info(p);
            hash = qHashMulti(hash, info.size(), info.lastModified().toMSecsSinceEpoch());
        }
        emitIfChanged(hash);
    }

    static QSet<QString> parseDeclared(const QString &qmldirPath) {
        QSet<QString> files;
        QFile f(qmldirPath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return files;
        const QString base = QFileInfo(qmldirPath).absolutePath();
        QTextStream in(&f);
        while (!in.atEnd()) {
            const QStringList parts = in.readLine().trimmed().split(' ', Qt::SkipEmptyParts);
            if (parts.isEmpty() || parts.first().startsWith('#')) continue;
            const QString last = parts.last();
            if (last.endsWith(".qml") || last.endsWith(".js") || last.endsWith(".mjs")) {
                const QString abs = QDir(base).absoluteFilePath(last);
                if (QFileInfo::exists(abs)) files << abs;
            }
        }
        return files;
    }

    static ScanResult crawl(const QString &path) {
        ScanResult result;
        QDirIterator it(path, {"qmldir"}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString qmldirPath = it.next();
            result.files << qmldirPath;
            result.files += parseDeclared(qmldirPath);
        }
        size_t hash = 0;
        for (const QString &p : std::as_const(result.files)) {
            const QFileInfo info(p);
            hash = qHashMulti(hash, info.size(), info.lastModified().toMSecsSinceEpoch());
        }
        result.hash = hash;
        return result;
    }

    bool               m_enabled     = true;
    bool               m_ready       = false;
    bool               m_initialized = false;
    bool               m_pendingFull = false;
    size_t             m_stateHash   = 0;
    QString            m_path;
    QSet<QString>      m_watched;
    QFileSystemWatcher m_sysWatcher;
    QTimer             m_debounce;
    QFutureWatcher<ScanResult> m_watcher;
};
