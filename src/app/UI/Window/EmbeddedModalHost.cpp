#include "EmbeddedModalHost.h"

#include <QColor>
#include <QMouseEvent>
#include <QPainter>
#include <QShortcut>

#include <lite/GUI/Theme/ThemeManager.h>

namespace {
    constexpr qreal modalPanelCornerRadius = 8.0;
}

EmbeddedModalHost::EmbeddedModalHost(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    hide();

    m_panel = new QWidget(this);
    m_panel->setObjectName(QStringLiteral("EmbeddedModalPanel"));
    m_panel->setAttribute(Qt::WA_StyledBackground);

    applyTheme();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this,
            [this](const QString &) { applyTheme(); });

    m_escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    m_escShortcut->setEnabled(false);
    connect(m_escShortcut, &QShortcut::activated, this, &EmbeddedModalHost::closePanel);
}

void EmbeddedModalHost::open(QWidget *content, const QSize &panelSize) {
    if (m_open) {
        // Already open: only swap the content and keep the current state.
        if (content && content != m_content) {
            m_content = content;
            m_content->setParent(m_panel);
            m_panelSize = panelSize.isValid() ? panelSize : m_content->sizeHint();
            resetPanelGeometry();
            anchorPanelToCenter();
        }
        if (m_content)
            m_content->setFocus();
        emit visualStateChanged();
        return;
    }

    m_content = content;
    if (m_content)
        m_content->setParent(m_panel);
    m_panelSize = panelSize.isValid() ? panelSize : (m_content ? m_content->sizeHint() : QSize());
    resetPanelGeometry();
    anchorPanelToCenter();

    m_open = true;
    m_escShortcut->setEnabled(true);
    show();
    raise();
    emit opened();
    emit visualStateChanged();
    if (m_content)
        m_content->setFocus();
}

void EmbeddedModalHost::closePanel() {
    if (!m_open)
        return;
    m_open = false;
    m_escShortcut->setEnabled(false);
    hide();
    emit visualStateChanged();
    emit closed();
}

bool EmbeddedModalHost::isOpen() const {
    return m_open;
}

QRect EmbeddedModalHost::panelGeometry() const {
    return m_panel->geometry();
}

qreal EmbeddedModalHost::panelCornerRadius() const {
    return modalPanelCornerRadius;
}

QColor EmbeddedModalHost::backdropColor() const {
    return m_backdropColor;
}

void EmbeddedModalHost::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    if (!m_open)
        return;
    QPainter p(this);
    p.fillRect(rect(), m_backdropColor);
}

void EmbeddedModalHost::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (m_panelSize.isEmpty())
        return;
    // Keep the panel centered while the window changes size (including
    // maximize/restore); moving a hidden panel is harmless.
    anchorPanelToCenter();
    if (m_open)
        emit visualStateChanged();
}

void EmbeddedModalHost::mousePressEvent(QMouseEvent *event) {
    // Only clicks on the backdrop itself close the modal; presses that bubble
    // up from unhandled blank areas inside the panel must not close it.
    if (m_panel->rect().contains(m_panel->mapFromGlobal(event->globalPosition().toPoint()))) {
        event->accept();
        return;
    }
    closePanel();
    event->accept();
}

void EmbeddedModalHost::mouseDoubleClickEvent(QMouseEvent *event) {
    event->accept();
}

void EmbeddedModalHost::applyTheme() {
    auto color = ThemeManager::instance()->semanticColor(QStringLiteral("overlay.backdrop"));
    m_backdropColor = color.isValid() ? color : QColor(0, 0, 0, 96);
    if (m_open)
        update();
    emit visualStateChanged();
}

// Recomputes the centered position and moves the panel there (used on open
// and on resize / content swap while the panel is live).
void EmbeddedModalHost::anchorPanelToCenter() {
    m_panelRestPos = centerPosition(m_panelSize);
    m_panel->move(m_panelRestPos);
}

void EmbeddedModalHost::resetPanelGeometry() {
    m_panel->resize(m_panelSize);
    if (m_content)
        m_content->setGeometry(m_panel->rect());
}

QPoint EmbeddedModalHost::centerPosition(const QSize &size) const {
    return QPoint(qMax(0, (width() - size.width()) / 2), qMax(0, (height() - size.height()) / 2));
}
