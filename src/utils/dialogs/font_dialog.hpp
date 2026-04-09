#pragma once

#include <QObject>
#include <QFont>
#include <QFontDialog>
#include <QtQmlIntegration/qqmlintegration.h>

class FontDialog : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QFont selectedFont READ selectedFont NOTIFY selectedFontChanged)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(bool ok READ ok NOTIFY okChanged)

public:
    explicit FontDialog(QObject *parent = nullptr);
    ~FontDialog();

    Q_INVOKABLE void open();
    Q_INVOKABLE void accept();
    Q_INVOKABLE void reject();

    QFont selectedFont() const;
    QString title() const;
    void setTitle(const QString &title);
    bool ok() const;

signals:
    void selectedFontChanged();
    void titleChanged();
    void okChanged();
    void fontSelected(const QFont &font);
    void finished(int result);

private:
    void setupDialog();

    QFontDialog *m_dialog;
    QFont m_selectedFont;
    QString m_title;
    bool m_ok;
};
