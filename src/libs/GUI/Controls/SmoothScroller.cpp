#include "SmoothScroller.h"

#include <QAbstractScrollArea>
#include <QAbstractItemView>
#include <QApplication>
#include <QEvent>
#include <QScrollBar>
#include <QWheelEvent>

SmoothScroller::SmoothScroller(QObject *parent) : QObject(parent) {
    m_wheelInput.setContinuousInputMode(WheelInputController::ContinuousInputMode::PassThrough);

    const auto installTarget = [this](const Qt::Orientation orientation) {
        m_wheelInput.setScrollTarget(
            orientation,
            {
                .value =
                    [this, orientation] {
                        const auto *bar = scrollBar(orientation);
                        return bar ? static_cast<double>(bar->value()) : 0.0;
                    },
                .setValue =
                    [this, orientation](const double value) {
                        if (auto *bar = scrollBar(orientation))
                            bar->setValue(qRound(value));
                    },
                .boundedValue =
                    [this, orientation](const double value) {
                        const auto *bar = scrollBar(orientation);
                        return bar ? static_cast<double>(
                                         qBound(bar->minimum(), qRound(value), bar->maximum()))
                                   : 0.0;
                    },
                .step = [this, orientation] { return scrollStep(orientation); },
                .canScroll = [this, orientation] { return canAnimateScroll(orientation); },
            });
    };
    installTarget(Qt::Horizontal);
    installTarget(Qt::Vertical);
}

void SmoothScroller::attachTo(QAbstractScrollArea *area) {
    if (m_area)
        m_area->viewport()->removeEventFilter(this);
    m_wheelInput.stop();
    m_area = area;
    auto *viewport = area->viewport();
    viewport->installEventFilter(this);
    viewport->setMouseTracking(true);
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

    if (m_wheelInput.handleWheel(wheelEvent))
        return true;
    return QObject::eventFilter(watched, event);
}

QScrollBar *SmoothScroller::scrollBar(const Qt::Orientation orientation) const {
    if (!m_area)
        return nullptr;
    return orientation == Qt::Horizontal ? m_area->horizontalScrollBar()
                                         : m_area->verticalScrollBar();
}

bool SmoothScroller::canAnimateScroll(const Qt::Orientation orientation) const {
    const auto *bar = scrollBar(orientation);
    if (!bar || bar->maximum() <= bar->minimum())
        return false;
    if (orientation == Qt::Vertical) {
        const auto *itemView = qobject_cast<QAbstractItemView *>(m_area);
        if (itemView && itemView->verticalScrollMode() == QAbstractItemView::ScrollPerItem)
            return false;
    }
    return true;
}

double SmoothScroller::scrollStep(const Qt::Orientation orientation) const {
    if (!m_area)
        return 0.0;
    auto step = (orientation == Qt::Horizontal ? m_area->viewport()->width()
                                               : m_area->viewport()->height()) *
                0.15;
    if (orientation == Qt::Vertical) {
        const auto *itemView = qobject_cast<QAbstractItemView *>(m_area);
        if (itemView && itemView->verticalScrollMode() == QAbstractItemView::ScrollPerPixel) {
            const auto rowHeight = itemView->sizeHintForRow(0);
            const auto scrollLines = QApplication::wheelScrollLines();
            if (rowHeight > 0 && scrollLines > 0)
                step = rowHeight * scrollLines;
        }
    }
    return step;
}
