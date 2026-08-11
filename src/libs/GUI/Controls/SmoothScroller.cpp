#include "SmoothScroller.h"

#include <QAbstractScrollArea>
#include <QAbstractItemView>
#include <QApplication>
#include <QEvent>
#include <QScrollBar>
#include <QWheelEvent>

SmoothScroller::SmoothScroller(QObject *parent) : QObject(parent) {
    m_hAnim.setEasingCurve(QEasingCurve::OutCubic);
    m_vAnim.setEasingCurve(QEasingCurve::OutCubic);
    m_hAnim.setPropertyName("value");
    m_vAnim.setPropertyName("value");
    connect(&m_hAnim, &QPropertyAnimation::finished, this, [this] { m_logicalH.reset(); });
    connect(&m_vAnim, &QPropertyAnimation::finished, this, [this] { m_logicalV.reset(); });
    updateAnimationDuration();
    initializeAnimation();
}

void SmoothScroller::attachTo(QAbstractScrollArea *area) {
    m_area = area;
    auto *viewport = area->viewport();
    viewport->installEventFilter(this);
    viewport->setMouseTracking(true);
    m_hAnim.setTargetObject(area->horizontalScrollBar());
    m_vAnim.setTargetObject(area->verticalScrollBar());
    // Existing OverlayScrollBars are unaffected end-to-end: the animation drives
    // the QAbstractItemView/QScrollArea scrollbar value, and OverlayScrollBar
    // mirrors that value 1:1 through its valueChanged connection.
}

bool SmoothScroller::eventFilter(QObject *watched, QEvent *event) {
    if (!m_area || watched != m_area->viewport() || event->type() != QEvent::Wheel) {
        return QObject::eventFilter(watched, event);
    }

    auto *wheelEvent = static_cast<QWheelEvent *>(event);

    // Pass through wheel events carrying modifiers: Ctrl zooms fonts
    // (PhonicTextEdit / LyricWrapView) and Shift/Alt are handled by the widget itself.
    if (wheelEvent->modifiers() != Qt::NoModifier)
        return QObject::eventFilter(watched, event);

    // Record this step into the sliding window
    const auto &d = wheelEvent->angleDelta();
    const auto step = (qAbs(d.x()) == 0 && qAbs(d.y()) % 120 == 0) ||
                      (qAbs(d.x()) % 120 == 0 && qAbs(d.y()) == 0);
    if (m_stepWindow.size() == kWindowSize)
        m_stepWindow.removeFirst();
    m_stepWindow.append(step);

    // Any non-120-multiple event in the window means touchpad: pass through until that
    // event falls out of the window. Default mouse animation on an empty window
    // (all_of on an empty window is true), same direction as the editor view.
    if (!std::all_of(m_stepWindow.cbegin(), m_stepWindow.cend(), [](bool v) { return v; })) {
        return QObject::eventFilter(watched, event); // do not consume; native scrolling
    }

    // Mouse wheel: animate the scrollbar value
    const auto delta = wheelEvent->angleDelta();
    const bool horizontal = (qAbs(delta.x()) > qAbs(delta.y()));
    auto *bar = horizontal ? m_area->horizontalScrollBar() : m_area->verticalScrollBar();
    if (!bar || bar->maximum() <= bar->minimum())
        return QObject::eventFilter(watched, event); // no range; native handling

    // Per-line item views (ScrollPerItem, e.g. ComboBox's default popup list /
    // QListWidget) pass through untouched: cheap per-line scrolling suits long
    // lists and needs no animation.
    if (!horizontal && qobject_cast<QAbstractItemView *>(m_area) &&
        static_cast<QAbstractItemView *>(m_area)->verticalScrollMode() ==
            QAbstractItemView::ScrollPerItem) {
        return QObject::eventFilter(watched, event); // do not consume; native per-line scroll
    }
    // Pixel-scrolling views (ScrollPerPixel item views / QScrollArea):
    // perStep is "system wheel lines x row height" pixels (default 3 lines), matching
    // the native per-tick displacement so animated and unanimated scrolling stay aligned.
    // Views without rows (QScrollArea) fall back to viewport x 0.15.
    double perStep =
        (horizontal ? m_area->viewport()->width() : m_area->viewport()->height()) * 0.15;
    if (!horizontal) {
        if (auto *itemView = qobject_cast<QAbstractItemView *>(m_area);
            itemView && itemView->verticalScrollMode() == QAbstractItemView::ScrollPerPixel) {
            const int rowHeight = itemView->sizeHintForRow(0);
            const int scrollLines = QApplication::wheelScrollLines(); // -1 = page scroll
            if (rowHeight > 0 && scrollLines > 0)
                perStep = rowHeight * scrollLines;
        }
    }
    QPropertyAnimation *anim = horizontal ? &m_hAnim : &m_vAnim;
    std::optional<int> *logical = horizontal ? &m_logicalH : &m_logicalV;

    const auto activeEnd = (anim->state() == QAbstractAnimation::Running && logical->has_value())
                               ? **logical
                               : bar->value();
    auto endValue = static_cast<int>(activeEnd - perStep * delta.y() / 120.0);
    endValue = qBound(bar->minimum(), endValue, bar->maximum());

    if (bar->value() != endValue) {
        anim->stop();
        anim->setStartValue(bar->value());
        anim->setEndValue(endValue);
        *logical = endValue;
        anim->start();
    }
    return true; // consume the wheel event
}

void SmoothScroller::updateAnimationDuration() {
    const auto duration = getEffectiveAnimationTime(kBaseMs, AnimationGlobal::Full);
    auto heat = [this, duration](QPropertyAnimation &anim, std::optional<int> &logical,
                                 QScrollBar *bar) {
        const bool running = anim.state() == QAbstractAnimation::Running;
        const auto endValue = anim.endValue();
        anim.stop();
        anim.setDuration(duration);
        if (!running)
            return;
        if (duration == 0) {
            bar->setValue(endValue.toInt());
            logical.reset();
            return;
        }
        anim.setStartValue(anim.currentValue());
        anim.setEndValue(endValue);
        *logical = endValue.toInt();
        anim.start();
    };
    heat(m_hAnim, m_logicalH, m_area ? m_area->horizontalScrollBar() : nullptr);
    heat(m_vAnim, m_logicalV, m_area ? m_area->verticalScrollBar() : nullptr);
}

void SmoothScroller::afterSetAnimationLevel(AnimationGlobal::AnimationLevels level) {
    Q_UNUSED(level)
    updateAnimationDuration();
}

void SmoothScroller::afterSetTimeScale(double scale) {
    Q_UNUSED(scale)
    updateAnimationDuration();
}
