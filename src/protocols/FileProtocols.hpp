#pragma once
#include <QObject>
#include <QString>
#include <QByteArray>
#include <QQmlEngine>
#include <QJSEngine>

class FileProtocols : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    static FileProtocols* create(QQmlEngine* engine, QJSEngine*);

    Q_INVOKABLE bool exists(const QString& path) const;
    Q_INVOKABLE qint64 size(const QString& path) const;
    Q_INVOKABLE bool isReadable(const QString& path) const;
    Q_INVOKABLE bool isWritable(const QString& path) const;
    Q_INVOKABLE QString absolutePath(const QString& path) const;
    Q_INVOKABLE QString extension(const QString& path) const;

    Q_INVOKABLE QByteArray read(const QString& path) const;
    Q_INVOKABLE QString readText(const QString& path) const;

    Q_INVOKABLE bool write(const QString& path, const QByteArray& data, bool append = false) const;
    Q_INVOKABLE bool writeText(const QString& path, const QString& data, bool append = false) const;

    Q_INVOKABLE bool copy(const QString& src, const QString& dst) const;
    Q_INVOKABLE bool move(const QString& src, const QString& dst) const;
    Q_INVOKABLE bool rename(const QString& oldPath, const QString& newPath) const;
    Q_INVOKABLE bool remove(const QString& path) const;

    Q_INVOKABLE bool ensureDir(const QString& path) const;
    Q_INVOKABLE bool removeDir(const QString& path, bool recursive = false) const;

private:
    explicit FileProtocols(QObject* parent);
    Q_DISABLE_COPY(FileProtocols)
};
