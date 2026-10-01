#include "ParamAnchorOverlayView.h"

#include "UI/Views/ClipEditor/AnchorEditor/AnchorEditController.h"
#include "UI/Views/Common/EditorPenController.h"

#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>

namespace {
    // This overlay only exists for the anchor tool, which is one of the tools an
    // eraser has nothing to do with (EditorPenPolicy answers Unsupported): any
    // erase hint at all means the stroke will be swallowed whole.
    [[nodiscard]] bool penRefusesStroke() {
        return EditorPenController::eraseHintActive();
    }
}

ParamAnchorOverlayView::ParamAnchorOverlayView(AnchorEditor::AnchorEditController *controller,
                                               ValueMapper valueToSceneY, ValueMapper sceneYToValue)
    : AnchorOverlayView(std::move(valueToSceneY), std::move(sceneYToValue)),
      m_controller(controller) {
    setDisplayMode(PitchDisplayMode::Anchor);
    setVisible(false);
}

void ParamAnchorOverlayView::setInteractive(const bool interactive) {
    setTransparentMouseEvents(!interactive);
    setAcceptedMouseButtons(interactive ? Qt::AllButtons : Qt::NoButton);
}

void ParamAnchorOverlayView::setAnchorsVisible(const bool visible) {
    setVisible(visible);
}

void ParamAnchorOverlayView::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (transparentMouseEvents()) {
        event->ignore();
        return;
    }
    if (m_controller->pressAt(event->scenePos(), event->button()))
        event->accept();
}

void ParamAnchorOverlayView::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    if (transparentMouseEvents()) {
        event->ignore();
        return;
    }
    // Anchor editing is one of the tools an eraser has nothing to do with, so
    // while the pen offers one the stroke is swallowed whole (see
    // EditorPenPolicy) and the dashed insert preview describes an anchor that
    // could never be created.
    if (event->buttons() == Qt::NoButton && penRefusesStroke()) {
        m_controller->suspendHoverFeedback();
        event->accept();
        return;
    }
    if (m_controller->moveAt(event->scenePos(), event->buttons())) {
        if (const auto axes = m_controller->edgeAutoScrollAxes())
            emit autoScrollRequested(axes);
        event->accept();
    }
}

void ParamAnchorOverlayView::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    if (transparentMouseEvents()) {
        event->ignore();
        return;
    }
    m_controller->releaseAt(event->scenePos(), event->button());
    emit autoScrollStopped();
    event->accept();
}

void ParamAnchorOverlayView::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
    if (transparentMouseEvents()) {
        event->ignore();
        return;
    }
    m_controller->doubleClickAt(event->scenePos(), event->button());
    event->accept();
}

void ParamAnchorOverlayView::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    if (transparentMouseEvents()) {
        event->ignore();
        return;
    }
    m_controller->hoverEnter();
    event->accept();
}

void ParamAnchorOverlayView::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    if (transparentMouseEvents()) {
        event->ignore();
        return;
    }
    if (penRefusesStroke()) {
        m_controller->suspendHoverFeedback();
        event->accept();
        return;
    }
    m_controller->hoverMoveAt(event->scenePos());
    event->accept();
}

void ParamAnchorOverlayView::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    if (transparentMouseEvents()) {
        event->ignore();
        return;
    }
    m_controller->hoverLeave();
    event->accept();
}

void ParamAnchorOverlayView::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
    if (transparentMouseEvents()) {
        event->ignore();
        return;
    }
    emit contextMenuRequested(event->scenePos(), event->screenPos());
    event->accept();
}
