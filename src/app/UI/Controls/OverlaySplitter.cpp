#include "OverlaySplitter.h"

#include <QMouseEvent>
#include <QPainter>
#include <QVariantAnimation>

// Half-width of the overlay grip's hit area (total width = kGripHalfWidth * 2).
static constexpr int kGripHalfWidth = 4;
// Hysteresis margin (in pixels) to prevent jitter when collapsing/expanding.
// Once collapsed, the widget must be dragged back beyond (minSize/2 + kHysteresis)
// before it expands again.
static constexpr int kCollapseHysteresis = 20;

OverlaySplitter::OverlaySplitter(Qt::Orientation orientation, QWidget *parent)
    : QSplitter(orientation, parent) {
    setAttribute(Qt::WA_StyledBackground, true);
    setHandleWidth(0);
}

OverlaySplitter::OverlaySplitter(QWidget *parent) : QSplitter(parent) {
    setHandleWidth(0);
}

void OverlaySplitter::ensureGrip() {
    if (m_grip)
        return;

    // The grip must be parented to this splitter's parent widget (a sibling
    // in the widget hierarchy), because QSplitter treats all direct children
    // as managed split panels, which would break the grip's positioning.
    auto *p = parentWidget();
    if (!p)
        return;

    m_grip = new SplitterOverlayGrip(this, p);
    m_grip->show();
    connect(this, &QSplitter::splitterMoved, this, &OverlaySplitter::updateGripPosition);
}

bool OverlaySplitter::event(QEvent *event) {
    // Defer grip creation until the splitter has a parent and is shown.
    if (event->type() == QEvent::ParentChange || event->type() == QEvent::Show) {
        ensureGrip();
        updateGripPosition();
    }
    return QSplitter::event(event);
}

void OverlaySplitter::resizeEvent(QResizeEvent *event) {
    QSplitter::resizeEvent(event);
    updateGripPosition();
}

void OverlaySplitter::updateGripPosition() {
    if (!m_grip || count() < 2 || !parentWidget())
        return;

    auto *first = widget(0);
    if (!first)
        return;

    // Position the grip centered on the boundary between widget(0) and widget(1).
    // Convert the boundary coordinate from splitter-local to parent-local space.
    if (orientation() == Qt::Horizontal) {
        int localX = first->geometry().right() + 1;
        QPoint posInParent = mapToParent(QPoint(localX, 0));
        m_grip->setGeometry(posInParent.x() - kGripHalfWidth, posInParent.y(),
                            kGripHalfWidth * 2, height());
    } else {
        int localY = first->geometry().bottom() + 1;
        QPoint posInParent = mapToParent(QPoint(0, localY));
        m_grip->setGeometry(posInParent.x(), posInParent.y() - kGripHalfWidth,
                            width(), kGripHalfWidth * 2);
    }
    // Ensure the grip stays above all sibling widgets.
    m_grip->raise();
}

SplitterOverlayGrip::SplitterOverlayGrip(OverlaySplitter *splitter, QWidget *parent)
    : QWidget(parent), m_splitter(splitter) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_Hover);
    setMouseTracking(true);
    setCursor(splitter->orientation() == Qt::Horizontal ? Qt::SplitHCursor : Qt::SplitVCursor);

    m_animation = new QVariantAnimation(this);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_highlightOpacity = value.toDouble();
        update();
    });
}

void SplitterOverlayGrip::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    if (m_highlightOpacity <= 0.0)
        return;

    QPainter p(this);
    auto color = QColor(255, 255, 255, qRound(80 * m_highlightOpacity));
    if (m_splitter->orientation() == Qt::Horizontal) {
        int cx = (width() - 2) / 2;
        p.fillRect(cx, 0, 2, height(), color);
    } else {
        int cy = (height() - 2) / 2;
        p.fillRect(0, cy, width(), 2, color);
    }
}

void SplitterOverlayGrip::enterEvent(QEnterEvent *event) {
    Q_UNUSED(event)
    m_hovered = true;
    setHighlightVisible(true);
}

void SplitterOverlayGrip::leaveEvent(QEvent *event) {
    Q_UNUSED(event)
    m_hovered = false;
    if (!m_dragging)
        setHighlightVisible(false);
}

void SplitterOverlayGrip::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragStartPos = event->globalPosition().toPoint();
        m_dragStartSizes = m_splitter->sizes();
        // Initialize hysteresis state from current sizes.
        m_collapsed0 = m_dragStartSizes[0] == 0;
        m_collapsed1 = m_dragStartSizes[1] == 0;
        setHighlightVisible(true);
    }
}

void SplitterOverlayGrip::mouseMoveEvent(QMouseEvent *event) {
    if (!m_dragging)
        return;

    auto delta = event->globalPosition().toPoint() - m_dragStartPos;
    auto sizes = m_dragStartSizes;
    if (sizes.size() < 2)
        return;

    int d = m_splitter->orientation() == Qt::Horizontal ? delta.x() : delta.y();
    int newFirst = sizes[0] + d;
    int newSecond = sizes[1] - d;

    // Clamp to child widget min/max size constraints.
    // Collapse with hysteresis: once collapsed, require dragging past an extra
    // margin before expanding again, to prevent jitter near the threshold.
    auto *w0 = m_splitter->widget(0);
    auto *w1 = m_splitter->widget(1);
    bool horizontal = m_splitter->orientation() == Qt::Horizontal;
    int minSize0 = horizontal ? w0->minimumWidth() : w0->minimumHeight();
    int minSize1 = horizontal ? w1->minimumWidth() : w1->minimumHeight();
    int maxSize0 = horizontal ? w0->maximumWidth() : w0->maximumHeight();
    int maxSize1 = horizontal ? w1->maximumWidth() : w1->maximumHeight();

    if (m_splitter->isCollapsible(0)) {
        int collapseAt = minSize0 / 2;
        int expandAt = collapseAt + kCollapseHysteresis;
        if (m_collapsed0)
            m_collapsed0 = newFirst < expandAt;
        else
            m_collapsed0 = newFirst < collapseAt;
        newFirst = m_collapsed0 ? 0 : qBound(minSize0, newFirst, maxSize0);
    } else {
        newFirst = qBound(minSize0, newFirst, maxSize0);
    }

    if (m_splitter->isCollapsible(1)) {
        int collapseAt = minSize1 / 2;
        int expandAt = collapseAt + kCollapseHysteresis;
        if (m_collapsed1)
            m_collapsed1 = newSecond < expandAt;
        else
            m_collapsed1 = newSecond < collapseAt;
        newSecond = m_collapsed1 ? 0 : qBound(minSize1, newSecond, maxSize1);
    } else {
        newSecond = qBound(minSize1, newSecond, maxSize1);
    }

    m_splitter->setSizes({newFirst, newSecond});
    m_splitter->updateGripPosition();
}

void SplitterOverlayGrip::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        if (!m_hovered)
            setHighlightVisible(false);
    }
}

void SplitterOverlayGrip::setHighlightVisible(bool visible) {
    m_animation->stop();
    m_animation->setStartValue(m_highlightOpacity);
    if (visible) {
        m_animation->setEndValue(1.0);
        m_animation->setDuration(100);
    } else {
        m_animation->setEndValue(0.0);
        m_animation->setDuration(300);
    }
    m_animation->start();
}
