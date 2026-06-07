#pragma once
#include <QCefView.h>
#include <QCefQuery.h>
#include <QCefEvent.h>
#include <QCefSetting.h>
#include <QCefConfig.h>
#include <QCefContext.h>
#include <QQuickPaintedItem>
#include <QPainter>
#include <QTimer>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QGuiApplication>
#include <QCoreApplication>

class CefWidget : public QQuickPaintedItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(CefWidget)
    Q_PROPERTY(QString url       READ url      WRITE setUrl       NOTIFY urlChanged)
    Q_PROPERTY(bool    loading   READ loading                      NOTIFY loadingChanged)
    Q_PROPERTY(bool    canGoBack READ canGoBack                    NOTIFY navigationChanged)
    Q_PROPERTY(bool    canGoFwd  READ canGoForward                 NOTIFY navigationChanged)
    Q_PROPERTY(QString title     READ title                        NOTIFY titleChanged)

public:
    explicit CefWidget(QQuickItem* p = nullptr)
        : QQuickPaintedItem(p)
    {
        initCefContext();

        setAcceptedMouseButtons(Qt::AllButtons);
        setFlag(ItemAcceptsInputMethod);
        setFlag(ItemIsFocusScope);
        setFlag(ItemClipsChildrenToShape);

        auto* s = new QCefSetting();
        s->setOffScreenRenderingEnabled(true);
        s->setHardwareAccelerationEnabled(true);
        s->setJavascript(true);

        m_view = new QCefView(QStringLiteral("about:blank"), s, nullptr);
        m_view->setAttribute(Qt::WA_DontShowOnScreen, true);
        m_view->setAttribute(Qt::WA_OpaquePaintEvent, true);
        m_view->resize(640, 480);

        connect(m_view, &QCefView::loadingStateChanged, this, [this](auto, bool l, bool b, bool f) {
            m_loading = l; m_canGoBack = b; m_canGoFwd = f;
            Q_EMIT loadingChanged(); Q_EMIT navigationChanged();
        });
        connect(m_view, &QCefView::titleChanged, this, [this](const QString& t) {
            m_title = t; Q_EMIT titleChanged();
        });

        m_timer = new QTimer(this);
        m_timer->setInterval(16);
        connect(m_timer, &QTimer::timeout, this, [this] {
            if (m_dirty) {
                m_pixmap = m_view->grab();
                m_dirty = false;
            }
            if (!m_pixmap.isNull())
                update();
        });
        m_timer->start();
    }

    ~CefWidget() { if (m_view) m_view->deleteLater(); }

    QString url()          const { return m_url; }
    bool    loading()      const { return m_loading; }
    bool    canGoBack()    const { return m_canGoBack; }
    bool    canGoForward() const { return m_canGoFwd; }
    QString title()        const { return m_title; }

    void setUrl(const QString& u) {
        if (m_url == u) return;
        m_url = u;
        m_view->navigateToUrl(u);
        Q_EMIT urlChanged();
    }

    Q_INVOKABLE void goBack()    { m_view->browserGoBack(); }
    Q_INVOKABLE void goForward() { m_view->browserGoForward(); }
    Q_INVOKABLE void reload()    { m_view->browserReload(); }
    Q_INVOKABLE void stop()      { m_view->browserStopLoad(); }

    Q_INVOKABLE void runJavaScript(const QString& js) {
        m_view->executeJavascript(QCefView::MainFrameID, js, QString());
    }

    Q_INVOKABLE void triggerEvent(const QString& name, const QVariantList& args = {}) {
        QCefEvent e(name);
        e.setArguments(args);
        m_view->triggerEvent(e);
    }

    void paint(QPainter* painter) override {
        if (!m_pixmap.isNull() && !m_pixmap.size().isEmpty())
            painter->drawPixmap(boundingRect(), m_pixmap, m_pixmap.rect());
    }

protected:
    void geometryChange(const QRectF& n, const QRectF&) override {
        QQuickPaintedItem::geometryChange(n, {});
        if (m_view) {
            m_view->resize(n.size().toSize());
            m_dirty = true;
        }
    }

    void mousePressEvent(QMouseEvent* e) override {
        forceActiveFocus();
        QCoreApplication::sendEvent(m_view, e);
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        QCoreApplication::sendEvent(m_view, e);
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        QCoreApplication::sendEvent(m_view, e);
    }
    void wheelEvent(QWheelEvent* e) override {
        QCoreApplication::sendEvent(m_view, e);
    }
    void keyPressEvent(QKeyEvent* e) override {
        QCoreApplication::sendEvent(m_view, e);
    }
    void keyReleaseEvent(QKeyEvent* e) override {
        QCoreApplication::sendEvent(m_view, e);
    }
    void inputMethodEvent(QInputMethodEvent* e) override {
        QCoreApplication::sendEvent(m_view, e);
    }
    QVariant inputMethodQuery(Qt::InputMethodQuery q) const override {
        return m_view->inputMethodQuery(q);
    }

    static void initCefContext() {
        if (QCefContext::instance()) return;

        auto cfg = new QCefConfig();
        cfg->setWindowlessRenderingEnabled(true);
        cfg->setLogLevel(QCefConfig::LOGSEVERITY_DISABLE);

        auto app = QCoreApplication::instance();
        auto args = app->arguments();
        auto vec = std::vector<QByteArray>(args.size());
        auto argv = std::vector<char*>(args.size());
        for (size_t i = 0; i < args.size(); ++i) {
            vec[i] = args[i].toLocal8Bit();
            argv[i] = vec[i].data();
        }
        int argc = args.size();

        m_ctx = new QCefContext(app, argc, argv.data(), cfg);
    }

Q_SIGNALS:
    void urlChanged();
    void loadingChanged();
    void navigationChanged();
    void titleChanged();

private:
    static inline QCefContext* m_ctx = nullptr;
    QCefView* m_view;
    QTimer* m_timer;
    QPixmap m_pixmap;
    QString m_url;
    QString m_title;
    bool m_loading = false, m_canGoBack = false, m_canGoFwd = false, m_dirty = true;
};
