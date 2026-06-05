#pragma once

#include <QtQuick/QQuickPaintedItem>
#include <QPainter>
#include <QImage>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QtConcurrent>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <mutex>
#include <atomic>

// Qt macros conflict with sigc++/pangomm (used by clatexmath)
#undef emit
#undef slots
#undef foreach

#include <clatexmath/latex.h>
#include <clatexmath/render.h>
#include <clatexmath/core/formula.h>
#include <clatexmath/platform/cairo/graphic_cairo.h>

class LatexRenderer : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString expression READ expression WRITE setExpression NOTIFY expressionChanged)
    Q_PROPERTY(QString cacheDir READ cacheDir WRITE setCacheDir NOTIFY cacheDirChanged)
    Q_PROPERTY(qreal textSize READ textSize WRITE setTextSize NOTIFY textSizeChanged)
    Q_PROPERTY(QColor foreground READ foreground WRITE setForeground NOTIFY foregroundChanged)
    Q_PROPERTY(QColor background READ background WRITE setBackground NOTIFY backgroundChanged)
    Q_PROPERTY(bool cacheEnabled READ cacheEnabled WRITE setCacheEnabled NOTIFY cacheEnabledChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)

public:
    explicit LatexRenderer(QQuickItem *parent = nullptr)
        : QQuickPaintedItem(parent)
    {
        setImplicitSize(100, 30);
        std::call_once(s_initFlag, [] {
            tex::LaTeX::init(s_resRoot.isEmpty() ? "/usr/share/clatexmath" : s_resRoot.toStdString());
        });
    }

    ~LatexRenderer() override
    {
        if (m_watcher) { m_watcher->cancel(); m_watcher->waitForFinished(); }
    }

    static void initialize(const QString &resRoot = {})
    {
        if (!resRoot.isEmpty()) s_resRoot = resRoot;
        std::call_once(s_initFlag, [] {
            tex::LaTeX::init(s_resRoot.isEmpty() ? "/usr/share/clatexmath" : s_resRoot.toStdString());
        });
    }

    static void shutdown() { tex::LaTeX::release(); }

    QString expression() const { return m_expression; }
    void setExpression(const QString &v) { if (m_expression == v) return; m_expression = v; Q_EMIT expressionChanged(); scheduleRender(); }

    QString cacheDir() const { return m_cacheDir.isEmpty() ? defaultCacheDir() : m_cacheDir; }
    void setCacheDir(const QString &v) { if (m_cacheDir == v) return; m_cacheDir = v; Q_EMIT cacheDirChanged(); }

    qreal textSize() const { return m_textSize; }
    void setTextSize(qreal v) { if (qFuzzyCompare(m_textSize, v)) return; m_textSize = v; Q_EMIT textSizeChanged(); scheduleRender(); }

    QColor foreground() const { return m_foreground; }
    void setForeground(const QColor &v) { if (m_foreground == v) return; m_foreground = v; Q_EMIT foregroundChanged(); scheduleRender(); }

    QColor background() const { return m_background; }
    void setBackground(const QColor &v) { if (m_background == v) return; m_background = v; Q_EMIT backgroundChanged(); scheduleRender(); }

    bool cacheEnabled() const { return m_cacheEnabled; }
    void setCacheEnabled(bool v) { if (m_cacheEnabled == v) return; m_cacheEnabled = v; Q_EMIT cacheEnabledChanged(); }

    bool busy() const { return m_busy; }

    Q_INVOKABLE void clearCache() {
        QDir d(cacheDir()); if (!d.exists()) return;
        for (const auto &fi : d.entryInfoList({"*.png"}, QDir::Files))
            QFile::remove(fi.absoluteFilePath());
    }

    Q_INVOKABLE QString hashFor(const QString &e) const { return computeHash(e, m_textSize, m_foreground); }

    Q_INVOKABLE bool isCached(const QString &e) const {
        return !e.isEmpty() && QFile::exists(cacheFilePath(computeHash(e, m_textSize, m_foreground)));
    }

    Q_INVOKABLE void preRender(const QString &e) {
        if (e.isEmpty()) return;
        (void)QtConcurrent::run([this, e] {
            QString h = computeHash(e, m_textSize, m_foreground);
            if (m_cacheEnabled && !loadFromCache(h).isNull()) { Q_EMIT cached(h, cacheFilePath(h)); return; }
            QImage img = renderExpression(e, m_textSize, m_foreground, m_background);
            if (!img.isNull() && m_cacheEnabled) { saveToCache(h, img); Q_EMIT cached(h, cacheFilePath(h)); }
        });
    }

    void paint(QPainter *p) override { if (!m_image.isNull()) p->drawImage(0, 0, m_image); }

signals:
    void expressionChanged();
    void cacheDirChanged();
    void textSizeChanged();
    void foregroundChanged();
    void backgroundChanged();
    void cacheEnabledChanged();
    void busyChanged();
    void renderError(const QString &msg);
    void renderStarted();
    void renderFinished();
    void cached(const QString &hash, const QString &filePath);

private:
    void scheduleRender()
    {
        if (m_expression.isEmpty()) return;
        if (m_watcher) { m_watcher->cancel(); m_watcher->deleteLater(); m_watcher = nullptr; }

        QString hash = computeHash(m_expression, m_textSize, m_foreground);

        if (m_cacheEnabled) {
            m_image = loadFromCache(hash);
            if (!m_image.isNull()) { setImplicitSize(m_image.width(), m_image.height()); update(); return; }
        }

        Q_EMIT renderStarted();
        m_busy = true; Q_EMIT busyChanged();

        auto self = QPointer<LatexRenderer>(this);
        QString expr = m_expression;
        qreal size = m_textSize;
        QColor fg = m_foreground, bg = m_background;
        bool cache = m_cacheEnabled;
        QString cDir = m_cacheDir;

        m_watcher = new QFutureWatcher<QImage>(this);
        connect(m_watcher, &QFutureWatcher<QImage>::finished, this, [this, self, hash, cache, cDir] {
            if (!self || !m_watcher) return;
            m_image = m_watcher->result();
            m_watcher->deleteLater(); m_watcher = nullptr;
            if (!m_image.isNull()) {
                setImplicitSize(m_image.width(), m_image.height());
                if (cache) { saveToCache(hash, m_image); Q_EMIT cached(hash, cacheDir() + "/" + hash + ".png"); }
            } else Q_EMIT renderError(QStringLiteral("Rendering returned empty result"));
            m_busy = false; Q_EMIT busyChanged(); Q_EMIT renderFinished(); update();
        });

        m_watcher->setFuture(QtConcurrent::run([self, expr, size, fg, bg]() -> QImage {
            if (!self) return {};
            return self->renderExpression(expr, size, fg, bg);
        }));
    }

    QImage renderExpression(const QString &expr, qreal size, const QColor &fg, const QColor &bg)
    {
        if (expr.isEmpty()) return {};
        std::lock_guard<std::mutex> lock(s_renderMutex);
        try {
            tex::Formula formula(expr.toStdWString());
            auto *render = tex::TeXRenderBuilder{}
                .setStyle(tex::TexStyle::display)
                .setTextSize(static_cast<float>(size))
                .setWidth(tex::UnitType::pixel, 2000.f, tex::Alignment::left)
                .setIsMaxWidth(true)
                .setLineSpace(tex::UnitType::pixel, static_cast<float>(size / 3.))
                .setForeground(((tex::color)(fg.alpha()) << 24) | ((tex::color)(fg.red()) << 16) | ((tex::color)(fg.green()) << 8) | (tex::color)(fg.blue()))
                .build(formula);
            if (!render) return {};

            int w = render->getWidth(), h = render->getHeight();
            if (w < 1 || h < 1) { delete render; return {}; }

            auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, w, h);
            auto cr = Cairo::Context::create(surface);

            if (bg.alpha() > 0) {
                cr->save(); cr->set_source_rgba(bg.redF(), bg.greenF(), bg.blueF(), bg.alphaF()); cr->paint(); cr->restore();
            }

            tex::Graphics2D_cairo g2(cr);
            render->draw(g2, 0, 0);

            QImage img(surface->get_data(), w, h, surface->get_stride(), QImage::Format_ARGB32);
            QImage copy = img.copy();
            delete render;
            return copy;
        } catch (const std::exception &e) {
            qWarning("LatexRenderer: %s", e.what());
            return {};
        }
    }

    QString computeHash(const QString &expr, qreal size, const QColor &fg) const
    {
        QByteArray d;
        QDataStream ds(&d, QIODevice::WriteOnly);
        ds.setByteOrder(QDataStream::LittleEndian);
        ds << expr << static_cast<double>(size) << static_cast<quint64>(fg.rgba());
        return QString::fromLatin1(QCryptographicHash::hash(d, QCryptographicHash::Sha256).toHex());
    }

    QString defaultCacheDir() const { return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/noon/latex"; }
    QString cacheFilePath(const QString &hash) const { return cacheDir() + "/" + hash + ".png"; }

    QImage loadFromCache(const QString &hash) const
    {
        QString path = cacheFilePath(hash);
        if (!QFile::exists(path)) return {};
        QImage img(path);
        if (img.isNull()) { QFile::remove(path); return {}; }
        return img;
    }

    void saveToCache(const QString &hash, const QImage &img)
    {
        QString dir = cacheDir(); QDir().mkpath(dir);
        QString path = dir + "/" + hash + ".png";
        if (QFile::exists(path)) return;
        img.save(path, "PNG");
    }

    QString m_expression;
    QString m_cacheDir;
    qreal m_textSize = 20.0;
    QColor m_foreground = Qt::black;
    QColor m_background = Qt::transparent;
    bool m_cacheEnabled = true;
    std::atomic<bool> m_busy{false};
    QImage m_image;
    QFutureWatcher<QImage> *m_watcher = nullptr;

    static inline std::once_flag s_initFlag;
    static inline QString s_resRoot;
    static inline std::mutex s_renderMutex;
};
