#pragma once
#include <QtQuick/QQuickPaintedItem>
#include <QImage>
#include <QPainter>
#include <QtQml/qqmlregistration.h>
#include "QrCodeGenerator.h"

class QrCode : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)

public:
    explicit QrCode(QQuickItem *parent = nullptr)
        : QQuickPaintedItem(parent) {}

    QString text() const { return m_text; }

    void setText(const QString &t) {
        if (m_text != t) {
            m_text = t;
            QrCodeGenerator gen;
            m_img = gen.generateQr(m_text);
            update();
            emit textChanged();
        }
    }

    void paint(QPainter *painter) override {
        if (m_img.isNull()) return;
        painter->setRenderHint(QPainter::Antialiasing, false);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter->drawImage(boundingRect(), m_img);
    }

signals:
    void textChanged();

private:
    QString m_text;
    QImage m_img;
};
