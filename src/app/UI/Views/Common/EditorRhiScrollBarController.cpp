#include "EditorRhiScrollBarController.h"

#include <lite/GUI/Controls/OverlayScrollBar.h>

#include <QWidget>

#include <algorithm>

EditorRhiScrollBarController::EditorRhiScrollBarController(QWidget *viewport, QObject *parent)
    : QObject(parent), m_viewport(viewport) {
    Q_ASSERT(m_viewport);
    m_horizontalBar = OverlayScrollBar::installOn(m_viewport, Qt::Horizontal);
    m_verticalBar = OverlayScrollBar::installOn(m_viewport, Qt::Vertical);
    m_horizontalBar->setCompanion(m_verticalBar);
    m_verticalBar->setCompanion(m_horizontalBar);

    const auto requestOffset = [this] {
        if (!m_updating)
            emit offsetChangeRequested(QPointF(m_horizontalBar->value(), m_verticalBar->value()));
    };
    connect(m_horizontalBar, &QScrollBar::valueChanged, this, requestOffset);
    connect(m_verticalBar, &QScrollBar::valueChanged, this, requestOffset);
}

void EditorRhiScrollBarController::setMetrics(const QSizeF &contentSize, const QPointF &offset,
                                              const QSizeF &singleStep) {
    if (!m_viewport)
        return;

    const auto viewportSize = m_viewport->size();
    const auto configure = [this](OverlayScrollBar *bar, const double contentLength,
                                  const int viewportLength, const double currentOffset,
                                  const double requestedSingleStep) {
        bar->setPageStep(std::max(1, viewportLength));
        bar->setSingleStep(std::max(
            1, qRound(requestedSingleStep > 0.0 ? requestedSingleStep : viewportLength / 10.0)));
        bar->setRange(0, qRound(std::max(0.0, contentLength - viewportLength)));
        bar->setValue(qRound(currentOffset));
    };

    m_updating = true;
    configure(m_horizontalBar, contentSize.width(), viewportSize.width(), offset.x(),
              singleStep.width());
    configure(m_verticalBar, contentSize.height(), viewportSize.height(), offset.y(),
              singleStep.height());
    m_updating = false;

    m_horizontalBar->setRangeVisible(true);
    m_verticalBar->setRangeVisible(true);
    m_horizontalBar->updatePosition();
    m_verticalBar->updatePosition();
}

OverlayScrollBar *EditorRhiScrollBarController::horizontalBar() const {
    return m_horizontalBar;
}

OverlayScrollBar *EditorRhiScrollBarController::verticalBar() const {
    return m_verticalBar;
}
