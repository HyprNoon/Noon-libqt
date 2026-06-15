#pragma once
#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <QFile>
#include <QTextStream>
#include <QtQml/qqmlregistration.h>

class TomlParser : public QObject {
    Q_OBJECT
    QML_ELEMENT

public:
    explicit TomlParser(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE QVariantMap parse(const QString &filePath) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        return parseString(QString::fromUtf8(file.readAll()));
    }

    Q_INVOKABLE QVariantMap parseString(const QString &toml) {
        QVariantMap root;
        QStringList sectionPath;

        const auto lines = toml.split('\n');
        for (const auto &raw : lines) {
            QString line = raw.trimmed();
            if (line.isEmpty() || line.startsWith('#'))
                continue;

            if (line.startsWith('[')) {
                int close = line.lastIndexOf(']');
                if (close < 0) continue;
                QString header = line.mid(1, close - 1).trimmed();
                sectionPath = header.split('.');
                ensurePath(root, sectionPath);
                continue;
            }

            int eq = line.indexOf('=');
            if (eq < 0) continue;

            QString keyStr = line.left(eq).trimmed();
            QString valStr = line.mid(eq + 1).trimmed();
            QStringList keys = parseKeyPath(keyStr);
            QStringList fullPath = sectionPath + keys;
            deepSet(root, fullPath, parseValue(valStr));
        }
        return root;
    }

    Q_INVOKABLE QVariantList parseTemplates(const QString &filePath) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};
        return parseTemplatesString(QString::fromUtf8(file.readAll()));
    }

    Q_INVOKABLE QVariantList parseTemplatesString(const QString &toml) {
        QVariantList entries;
        const auto lines = toml.split('\n');

        QString currentName;
        QVariantMap currentEntry;
        bool inTemplatesPlain = false;
        bool ignoreNext = false;

        auto flush = [&] {
            if (!currentName.isEmpty()) {
                if (!currentEntry.contains(QStringLiteral("enabled")))
                    currentEntry[QStringLiteral("enabled")] = true;
                currentEntry[QStringLiteral("name")] = currentName;
                entries.append(currentEntry);
                currentEntry.clear();
                currentName.clear();
            }
        };

        for (const auto &raw : lines) {
            QString line = raw.trimmed();
            if (line.isEmpty())
                continue;

            if (line.startsWith(QStringLiteral("#IGNORE"))) {
                ignoreNext = true;
                continue;
            }

            if (line.startsWith('#'))
                continue;

            if (line.startsWith('[')) {
                flush();
                inTemplatesPlain = false;
                int close = line.lastIndexOf(']');
                if (close < 0) continue;
                QString header = line.mid(1, close - 1).trimmed();
                if (header == QStringLiteral("templates")) {
                    inTemplatesPlain = !ignoreNext;
                } else if (header.startsWith(QStringLiteral("templates.")) && !ignoreNext) {
                    currentName = header.mid(10);
                }
                ignoreNext = false;
                continue;
            }

            ignoreNext = false;

            int eq = line.indexOf('=');
            if (eq < 0) continue;

            if (!currentName.isEmpty()) {
                QString key = line.left(eq).trimmed();
                QString val = line.mid(eq + 1).trimmed();
                int vi = 0;
                currentEntry[key] = parseValue(val, vi);
                continue;
            }

            if (inTemplatesPlain) {
                QString name = line.left(eq).trimmed();
                QString valStr = line.mid(eq + 1).trimmed();
                int vi = 0;
                QVariant val = parseValue(valStr, vi);
                if (val.userType() == QMetaType::QVariantMap) {
                    QVariantMap entry = val.toMap();
                    if (!entry.contains(QStringLiteral("enabled")))
                        entry[QStringLiteral("enabled")] = true;
                    entry[QStringLiteral("name")] = name;
                    entries.append(entry);
                }
                continue;
            }
        }
        flush();
        return entries;
    }

    Q_INVOKABLE bool addTemplate(const QString &filePath, const QString &name,
                                  const QString &inputPath, const QString &outputPath) {
        QFile file(filePath);
        if (!file.open(QIODevice::Append | QIODevice::Text))
            return false;
        QTextStream out(&file);
        out << "\n[templates." << name << "]\n"
            << "enabled = true\n"
            << "input_path = \"" << inputPath << "\"\n"
            << "output_path = \"" << outputPath << "\"\n";
        return true;
    }

    Q_INVOKABLE bool removeTemplate(const QString &filePath, const QString &name) {
        return setEnabled(filePath, name, false);
    }

    Q_INVOKABLE bool enableTemplate(const QString &filePath, const QString &name) {
        return setEnabled(filePath, name, true);
    }

private:
    static bool setEnabled(const QString &filePath, const QString &name, bool enabled) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;
        const auto lines = QString::fromUtf8(file.readAll()).split('\n');
        file.close();

        QStringList out;
        bool inTarget = false;
        QString value = enabled ? QStringLiteral("true") : QStringLiteral("false");
        QString sectionPattern = QStringLiteral("templates.") + name;

        for (const auto &line : lines) {
            QString trimmed = line.trimmed();

            if (trimmed.startsWith('[')) {
                inTarget = false;
                int close = trimmed.lastIndexOf(']');
                if (close > 0) {
                    QString header = trimmed.mid(1, close - 1).trimmed();
                    if (header == sectionPattern) {
                        inTarget = true;
                        out << line;
                        out << QStringLiteral("enabled = ") + value;
                        continue;
                    }
                }
                out << line;
                continue;
            }

            if (inTarget) {
                int eq = trimmed.indexOf('=');
                if (eq > 0) {
                    QString key = trimmed.left(eq).trimmed();
                    if (key == QStringLiteral("enabled"))
                        continue;
                }
            }

            out << line;
        }

        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;
        file.write(out.join('\n').toUtf8());
        return true;
    }

public:

    Q_INVOKABLE bool setTemplate(const QString &filePath, const QString &name,
                                  const QString &inputPath, const QString &outputPath) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;
        const auto lines = QString::fromUtf8(file.readAll()).split('\n');
        file.close();

        QStringList out;
        bool inTarget = false;
        bool found = false;
        QString sectionPattern = QStringLiteral("templates.") + name;

        auto captureIndent = [](const QString &l) -> QString {
            QString ind;
            for (const QChar &c : l) {
                if (c == ' ' || c == '\t') ind += c;
                else break;
            }
            return ind;
        };

        for (const auto &line : lines) {
            QString trimmed = line.trimmed();
            if (trimmed.isEmpty() || trimmed.startsWith('#')) {
                out << line;
                continue;
            }

            if (trimmed.startsWith('[')) {
                if (inTarget) inTarget = false;
                int close = trimmed.lastIndexOf(']');
                if (close > 0) {
                    QString header = trimmed.mid(1, close - 1).trimmed();
                    if (header == sectionPattern) {
                        inTarget = true;
                        found = true;
                    }
                }
                out << line;
                continue;
            }

            if (inTarget) {
                int eq = trimmed.indexOf('=');
                if (eq > 0) {
                    QString key = trimmed.left(eq).trimmed();
                    if (key == QStringLiteral("input_path")) {
                        out << captureIndent(line) + QStringLiteral("input_path = \"") + inputPath + QLatin1Char('"');
                        continue;
                    }
                    if (key == QStringLiteral("output_path")) {
                        out << captureIndent(line) + QStringLiteral("output_path = \"") + outputPath + QLatin1Char('"');
                        continue;
                    }
                }
            }

            out << line;
        }

        if (!found) {
            out << QStringLiteral("\n[templates.") + name + QStringLiteral("]\n")
                << QStringLiteral("enabled = true\n")
                << QStringLiteral("input_path = \"") + inputPath + QStringLiteral("\"\n")
                << QStringLiteral("output_path = \"") + outputPath + QLatin1Char('"');
        }

        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;
        file.write(out.join('\n').toUtf8());
        return true;
    }

private:
    static void ensurePath(QVariantMap &map, const QStringList &path, int depth = 0) {
        if (depth >= path.size()) return;
        const QString &key = path[depth];
        if (!map.contains(key) || map.value(key).isNull())
            map[key] = QVariantMap();
        QVariantMap sub = map[key].toMap();
        ensurePath(sub, path, depth + 1);
        map[key] = sub;
    }

    static void deepSet(QVariantMap &map, const QStringList &keys, const QVariant &value, int depth = 0) {
        if (depth >= keys.size()) return;
        const QString &key = keys[depth];
        if (depth == keys.size() - 1) {
            map[key] = value;
            return;
        }
        QVariantMap sub = map.value(key).toMap();
        deepSet(sub, keys, value, depth + 1);
        map[key] = sub;
    }

    static QStringList parseKeyPath(const QString &str) {
        QStringList result;
        int i = 0;
        skipSpace(str, i);
        while (i < str.size()) {
            result << parseKey(str, i);
            skipSpace(str, i);
            if (i < str.size() && str[i] == '.') {
                i++;
                skipSpace(str, i);
            } else {
                break;
            }
        }
        return result;
    }

    static QString parseKey(const QString &s, int &i) {
        skipSpace(s, i);
        if (i >= s.size()) return {};
        if (s[i] == '"' || s[i] == '\'')
            return parseQuotedString(s, i);
        int start = i;
        while (i < s.size() && (s[i].isLetterOrNumber() || s[i] == '_' || s[i] == '-'))
            i++;
        return s.mid(start, i - start);
    }

    static QVariant parseValue(const QString &s, int start = 0) {
        int i = start;
        skipSpace(s, i);
        if (i >= s.size()) return {};

        if (s[i] == '{')
            return QVariant(parseInlineTable(s, i));

        if (s[i] == '"' || s[i] == '\'')
            return parseQuotedString(s, i);

        int end = i;
        while (end < s.size() && !QChar(s[end]).isSpace()
               && s[end] != '#' && s[end] != ','
               && s[end] != ']' && s[end] != '}')
            end++;

        QString raw = s.mid(i, end - i).trimmed();
        if (raw.isEmpty()) return {};

        if (raw.compare("true", Qt::CaseInsensitive) == 0) return true;
        if (raw.compare("false", Qt::CaseInsensitive) == 0) return false;

        bool ok = false;
        qlonglong n = raw.toLongLong(&ok);
        if (ok) return n;

        double d = raw.toDouble(&ok);
        if (ok) return d;

        return raw;
    }

    static QVariantMap parseInlineTable(const QString &s, int &i) {
        QVariantMap result;
        i++;
        while (i < s.size()) {
            skipSpace(s, i);
            if (i >= s.size() || s[i] == '}') break;

            QString key = parseKey(s, i);
            skipSpace(s, i);
            if (i < s.size() && s[i] == '=') i++;
            skipSpace(s, i);

            result[key] = (i < s.size() && s[i] == '{')
                ? QVariant(parseInlineTable(s, i))
                : parseValue(s, i);

            skipSpace(s, i);
            if (i < s.size() && s[i] == ',') i++;
        }
        if (i < s.size() && s[i] == '}') i++;
        return result;
    }

    static QString parseQuotedString(const QString &s, int &i) {
        if (i >= s.size()) return {};
        QChar quote = s[i];
        if (quote != '"' && quote != '\'') return {};
        i++;
        QString result;
        while (i < s.size() && s[i] != quote) {
            if (quote == '"' && s[i] == '\\' && i + 1 < s.size()) {
                i++;
                switch (s[i].toLatin1()) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                case '\\': result += '\\'; break;
                case '"': result += '"'; break;
                default: result += s[i]; break;
                }
            } else {
                result += s[i];
            }
            i++;
        }
        if (i < s.size() && s[i] == quote) i++;
        return result;
    }

    static void skipSpace(const QString &s, int &i) {
        while (i < s.size() && QChar(s[i]).isSpace()) i++;
    }
};
