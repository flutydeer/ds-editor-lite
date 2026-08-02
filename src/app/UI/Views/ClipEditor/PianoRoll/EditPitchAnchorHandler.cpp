#include "EditPitchAnchorHandler.h"

#include "PianoRollContextMenuController.h"
#include "PianoRollGraphicsView.h"
#include "PianoRollGraphicsView_p.h"
#include "PitchEditorView.h"
#include "Controller/ClipController.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/EditSessionManager.h"
#include "UI/Views/ClipEditor/AnchorEditor/AnchorEditUtils.h"
#include "UI/Views/ClipEditor/AnchorEditor/AnchorOverlayView.h"

#include <lite/ProjectModel/AppModel/SingingClip.h>

#include <QContextMenuEvent>
#include <QHoverEvent>
#include <QKeyEvent>
#include <QMouseEvent>

EditPitchAnchorHandler::EditPitchAnchorHandler() {
    m_controller.setCoordinateMapper({
        [this](const double x) { return qRound(q->sceneXToTick(x)); },
        [this](const int tick) { return q->tickToSceneX(tick); },
        [this](const double y) { return qRound(d->m_anchorEditor->sceneYToValue(y)); },
        [this](const int value) { return d->m_anchorEditor->valueToSceneY(value); },
    });
    m_controller.setHostCallbacks({
        [this] { return beginEditSession(); },
        [this](const QList<AnchorCurve *> &curves) { publish(curves); },
        [this](const AnchorEditor::EditFinishReason reason) { finishEditSession(reason); },
        [this] { triggerRepaint(); },
    });
}

void EditPitchAnchorHandler::activate() {
    q->setMouseTracking(true);
    m_controller.setEditActive(true);
}

void EditPitchAnchorHandler::deactivate() {
    q->setMouseTracking(false);
    m_controller.setEditActive(false);
}

bool EditPitchAnchorHandler::mousePressEvent(QMouseEvent *event) {
    return m_controller.pressAt(q->mapToScene(event->position().toPoint()), event->button());
}

bool EditPitchAnchorHandler::mouseMoveEvent(QMouseEvent *event) {
    return m_controller.moveAt(q->mapToScene(event->position().toPoint()), event->buttons());
}

bool EditPitchAnchorHandler::mouseReleaseEvent(QMouseEvent *event) {
    return m_controller.releaseAt(q->mapToScene(event->position().toPoint()), event->button());
}

void EditPitchAnchorHandler::mouseDoubleClickEvent(QMouseEvent *event) {
    m_controller.doubleClickAt(q->mapToScene(event->position().toPoint()), event->button());
}

void EditPitchAnchorHandler::hoverEnterEvent(QHoverEvent *event) {
    Q_UNUSED(event)
    m_controller.hoverEnter();
}

void EditPitchAnchorHandler::hoverLeaveEvent(QHoverEvent *event) {
    Q_UNUSED(event)
    m_controller.hoverLeave();
}

bool EditPitchAnchorHandler::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        m_controller.exitEditing();
        return true;
    }
    if (event->key() == Qt::Key_Delete) {
        m_controller.deleteSelectedNodes();
        return true;
    }
    return false;
}

void EditPitchAnchorHandler::commit() {
    // Mutations are committed atomically by AnchorEditController.
}

void EditPitchAnchorHandler::discard() {
    m_controller.cancel();
}

Qt::Orientations EditPitchAnchorHandler::edgeAutoScrollAxes() const {
    return m_controller.edgeAutoScrollAxes();
}

void EditPitchAnchorHandler::continueDragAt(const QPoint &viewportPos) {
    m_controller.continueDragAtScene(q->mapToScene(viewportPos));
}

void EditPitchAnchorHandler::setAlwaysVisible(const bool visible) {
    m_controller.setAlwaysVisible(visible);
}

void EditPitchAnchorHandler::loadFromModel(const QList<AnchorCurve *> &curves) {
    m_controller.loadFromModel(curves);
}

const AnchorEditor::AnchorOverlayState &EditPitchAnchorHandler::overlayState() const {
    return m_controller.state();
}

bool EditPitchAnchorHandler::prepareMenuContext(QContextMenuEvent *event,
                                                PianoRollMenuContext &context) {
    AnchorEditor::MenuInfo info;
    if (!m_controller.prepareMenu(q->mapToScene(event->pos()), info))
        return false;
    context.target = PianoRollMenuContext::Target::Anchor;
    context.globalPos = event->globalPos();
    context.anchorInterpolationEnabled = info.interpolationEnabled;
    context.anchorMode = info.mixedInterpolation                     ? PianoRollAnchorMode::Mixed
                         : info.interpolation == AnchorNode::Linear  ? PianoRollAnchorMode::Linear
                         : info.interpolation == AnchorNode::Hermite ? PianoRollAnchorMode::Hermite
                                                                     : PianoRollAnchorMode::None;
    return true;
}

void EditPitchAnchorHandler::setSelectedInterpolation(const PianoRollAnchorMode mode) {
    if (mode == PianoRollAnchorMode::Linear)
        m_controller.setSelectedInterpolation(AnchorNode::Linear);
    else if (mode == PianoRollAnchorMode::Hermite)
        m_controller.setSelectedInterpolation(AnchorNode::Hermite);
}

void EditPitchAnchorHandler::deleteSelectedNodesFromMenu() {
    m_controller.deleteSelectedNodes();
}

bool EditPitchAnchorHandler::beginEditSession() {
    if (!d || !d->m_clip)
        return false;
    if (m_sessionId != 0)
        return true;
    if (editSessionManager->hasActiveTransaction())
        return false;
    m_sessionId = editSessionManager->beginTransaction(
        AppStatus::EditObjectType::Param, d->m_clip->id(), {}, {}, {}, {ParamInfo::Pitch});
    appStatus->currentEditObject = AppStatus::EditObjectType::Param;
    return m_sessionId != 0;
}

void EditPitchAnchorHandler::publish(const QList<AnchorCurve *> &curves) {
    if (!d || !d->m_clip)
        return;
    const auto *pitch = d->m_clip->params.getParamByName(ParamInfo::Pitch);
    auto edited = AnchorEditor::replaceAnchors(pitch->curves(Param::Edited), curves);
    clipController->onParamEdited(ParamInfo::Pitch, edited);
    qDeleteAll(edited);
}

void EditPitchAnchorHandler::finishEditSession(const AnchorEditor::EditFinishReason reason) {
    const auto sessionId = m_sessionId;
    m_sessionId = 0;
    if (sessionId != 0 && editSessionManager->hasActiveTransaction() &&
        editSessionManager->activeSession().sessionId == sessionId) {
        editSessionManager->endTransaction(sessionId,
                                           reason == AnchorEditor::EditFinishReason::Commit
                                               ? EditSessionEndReason::Commit
                                               : EditSessionEndReason::Discard);
    }
    if (!editSessionManager->hasActiveTransaction())
        appStatus->currentEditObject = AppStatus::EditObjectType::None;
}

void EditPitchAnchorHandler::triggerRepaint() const {
    if (!d)
        return;
    if (d->m_pitchEditor)
        d->m_pitchEditor->update();
    if (d->m_anchorEditor)
        d->m_anchorEditor->update();
}
