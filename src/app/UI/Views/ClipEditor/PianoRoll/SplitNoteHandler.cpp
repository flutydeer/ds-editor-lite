#include "SplitNoteHandler.h"

#include "NoteView.h"
#include "PianoRollGraphicsView.h"
#include "PianoRollGraphicsView_p.h"
#include "PianoRollGraphicsViewHelper.h"
#include "SplitLineIndicator.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "UI/Views/Common/TimeGraphicsScene.h"
#include "Utils/TimelineSnapUtils.h"

#include <QMouseEvent>

namespace Helper = PianoRollGraphicsViewHelper;

SplitNoteHandler::SplitNoteHandler() = default;

SplitNoteHandler::~SplitNoteHandler() = default;

void SplitNoteHandler::activate() {
    if (!m_indicator) {
        m_indicator = new SplitLineIndicator;
        m_indicator->setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
        auto *scene = dynamic_cast<TimeGraphicsScene *>(q->scene());
        scene->addCommonItem(m_indicator);
    }
    // Indicator is created lazily, so re-apply the themed color on each activation
    m_indicator->setLineColor(d->m_splitLineColor);
}

void SplitNoteHandler::deactivate() {
    if (m_indicator)
        m_indicator->clearState();
}

void SplitNoteHandler::applySplitLineColor(const QColor &color) {
    if (m_indicator)
        m_indicator->setLineColor(color);
}

bool SplitNoteHandler::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton)
        return true;

    const auto scenePos = q->mapToScene(event->position().toPoint());
    const auto tick = static_cast<int>(q->sceneXToTick(scenePos.x()) + d->m_offset);
    const auto noteView = d->noteViewAt(event->position().toPoint());

    if (noteView) {
        Helper::splitNote(noteView->id(), tick);
        if (m_indicator)
            m_indicator->clearState();
    }
    return true;
}

bool SplitNoteHandler::mouseMoveEvent(QMouseEvent *event) {
    const auto scenePos = q->mapToScene(event->position().toPoint());
    const auto tick = static_cast<int>(q->sceneXToTick(scenePos.x()) + d->m_offset);
    const auto noteView = d->noteViewAt(event->position().toPoint());
    updateIndicator(noteView, tick);
    return true;
}

void SplitNoteHandler::hoverLeaveEvent(QHoverEvent *event) {
    Q_UNUSED(event);
    if (m_indicator)
        m_indicator->clearState();
}

void SplitNoteHandler::hoverMoveEvent(QHoverEvent *event) {
    const auto scenePos = q->mapToScene(event->position().toPoint());
    const auto tick = static_cast<int>(q->sceneXToTick(scenePos.x()) + d->m_offset);
    const auto noteView = d->noteViewAt(event->position().toPoint());
    updateIndicator(noteView, tick);
    q->setCursor(Qt::ArrowCursor);
}

void SplitNoteHandler::updateIndicator(NoteView *noteView, int tick) {
    const auto quantizedTickLength = TimelineSnapUtils::quantizeToTicks(appStatus->pianoRollQuantize);
    const auto snappedTick = TimelineSnapUtils::snapNearest(tick, quantizedTickLength);
    const auto splitPos = snappedTick - d->m_offset;
    m_indicator->updateIndicator(noteView, splitPos);
    m_indicator->setLastTick(tick);
}
