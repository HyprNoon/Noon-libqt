#include "FileProtocols.hpp"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QIODevice>
#include <QTextStream>

FileProtocols* FileProtocols::create(QQmlEngine* engine, QJSEngine*) {
    return new FileProtocols(engine);
}

FileProtocols::FileProtocols(QObject* parent) : QObject(parent) {}

bool FileProtocols::exists(const QString& path) const {
    return QFile::exists(path);
}

qint64 FileProtocols::size(const QString& path) const {
    return QFileInfo(path).size();
}

bool FileProtocols::isReadable(const QString& path) const {
    return QFileInfo(path).isReadable();
}

bool FileProtocols::isWritable(const QString& path) const {
    return QFileInfo(path).isWritable();
}

QString FileProtocols::absolutePath(const QString& path) const {
    return QFileInfo(path).absolutePath();
}

QString FileProtocols::extension(const QString& path) const {
    return QFileInfo(path).suffix();
}

QByteArray FileProtocols::read(const QString& path) const {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QByteArray data = file.readAll();
    file.close();
    return data;
}

QString FileProtocols::readText(const QString& path) const {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QTextStream in(&file);
    const QString data = in.readAll();
    file.close();
    return data;
}

bool FileProtocols::write(const QString& path, const QByteArray& data, bool append) const {
    QFile file(path);
    QIODevice::OpenMode mode = QIODevice::WriteOnly;
    if (append) mode |= QIODevice::Append;
    if (!file.open(mode))
        return false;
    const bool ok = file.write(data) != -1;
    file.close();
    return ok;
}

bool FileProtocols::writeText(const QString& path, const QString& data, bool append) const {
    QFile file(path);
    QIODevice::OpenMode mode = QIODevice::WriteOnly | QIODevice::Text;
    if (append) mode |= QIODevice::Append;
    if (!file.open(mode))
        return false;
    QTextStream out(&file);
    out << data;
    out.flush();
    const bool ok = (out.status() == QTextStream::Ok);
    file.close();
    return ok;
}

bool FileProtocols::copy(const QString& src, const QString& dst) const {
    if (src == dst) return true;
    if (QFile::exists(dst))
        QFile::remove(dst);
    return QFile::copy(src, dst);
}

bool FileProtocols::move(const QString& src, const QString& dst) const {
    if (src == dst) return true;
    if (QFile::rename(src, dst))
        return true;
    if (QFile::exists(dst))
        QFile::remove(dst);
    if (!QFile::copy(src, dst))
        return false;
    return QFile::remove(src);
}

bool FileProtocols::rename(const QString& oldPath, const QString& newPath) const {
    return QFile::rename(oldPath, newPath);
}

bool FileProtocols::remove(const QString& path) const {
    return QFile::remove(path);
}

bool FileProtocols::ensureDir(const QString& path) const {
    return QDir().mkpath(path);
}

bool FileProtocols::removeDir(const QString& path, bool recursive) const {
    QDir dir(path);
    if (!dir.exists()) return false;
    if (recursive)
        return dir.removeRecursively();
    return QDir().rmdir(path);
}
