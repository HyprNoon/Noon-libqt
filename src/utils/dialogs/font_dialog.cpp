#include "font_dialog.hpp"
#include <QGuiApplication>
#include <QApplication>

FontDialog::FontDialog(QObject *parent)
    : QObject(parent)
    , m_dialog(nullptr)
    , m_ok(false)
{
    setupDialog();
}

FontDialog::~FontDialog()
{
    if (m_dialog) {
        delete m_dialog;
    }
}

void FontDialog::setupDialog()
{
    if (!m_dialog) {
        // Get parent window if available
        QWidget *parentWidget = nullptr;
        if (auto app = qobject_cast<QApplication*>(QGuiApplication::instance())) {
            if (app->activeWindow()) {
                parentWidget = app->activeWindow();
            }
        }

        m_dialog = new QFontDialog(m_selectedFont, parentWidget);
        m_dialog->setWindowTitle(m_title);

        connect(m_dialog, &QDialog::accepted, this, [this]() {
            m_selectedFont = m_dialog->selectedFont();
            m_ok = true;
            emit selectedFontChanged();
            emit okChanged();
            emit fontSelected(m_selectedFont);
            emit finished(1);
        });

        connect(m_dialog, &QDialog::rejected, this, [this]() {
            m_ok = false;
            emit okChanged();
            emit finished(0);
        });
    }
}

void FontDialog::open()
{
    setupDialog();
    m_dialog->open();
}

void FontDialog::accept()
{
    if (m_dialog) {
        m_dialog->accept();
    }
}

void FontDialog::reject()
{
    if (m_dialog) {
        m_dialog->reject();
    }
}

QFont FontDialog::selectedFont() const
{
    return m_selectedFont;
}

QString FontDialog::title() const
{
    return m_title;
}

void FontDialog::setTitle(const QString &title)
{
    if (m_title != title) {
        m_title = title;
        if (m_dialog) {
            m_dialog->setWindowTitle(title);
        }
        emit titleChanged();
    }
}

bool FontDialog::ok() const
{
    return m_ok;
}
