#include "SplitNoteHandler.h"

#include "NoteView.h"
#include "PianoRollGraphicsView.h"
#include "PianoRollGraphicsView_p.h"
#include "SplitLineIndicator.h"
#include "Controller/ClipController.h"
#include "Model/AppModel/Note.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "UI/Views/Common/TimeGraphicsScene.h"
#include "Utils/TimelineSnapUtils.h"

#include <QMouseEvent>

SplitNoteHandler::SplitNoteHandler() = default;

SplitNoteHandler::~SplitNoteHandler() = default;

void SplitNoteHandler::activate() {
    if (!m_indicator) {
        m_indicator = new SplitLineIndicator;
        m_indicator->setPixelsPerQuarterNote(ClipEditorGlobal::pixelsPerQuarterNote);
        auto *scene = dynamic_cast<TimeGraphicsScene *>(q->scene());
        scene->addCommonItem(m_indicator);
    }
}

void SplitNoteHandler::deactivate() {
    if (m_indicator)
        m_indicator->clearState();
}

void SplitNoteHandler::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton)
        return;

    const auto scenePos = q->mapToScene(event->position().toPoint());
    const auto tick = static_cast<int>(q->sceneXToTick(scenePos.x()) + d->m_offset);
    const auto noteView = d->noteViewAt(event->position().toPoint());

    if (noteView)
        splitNoteAtPosition(noteView, tick);
}

void SplitNoteHandler::mouseMoveEvent(QMouseEvent *event) {
    const auto scenePos = q->mapToScene(event->position().toPoint());
    const auto tick = static_cast<int>(q->sceneXToTick(scenePos.x()) + d->m_offset);
    const auto noteView = d->noteViewAt(event->position().toPoint());
    updateIndicator(noteView, tick);
}

void SplitNoteHandler::hoverMoveEvent(QHoverEvent *event) {
    const auto scenePos = q->mapToScene(event->position().toPoint());
    const auto tick = static_cast<int>(q->sceneXToTick(scenePos.x()) + d->m_offset);
    const auto noteView = d->noteViewAt(event->position().toPoint());
    updateIndicator(noteView, tick);
    q->setCursor(Qt::ArrowCursor);
}

void SplitNoteHandler::updateIndicator(NoteView *noteView, int tick) {
    const auto quantizedTickLength = TimelineSnapUtils::quantizeToTicks(appStatus->quantize);
    const auto snappedTick = TimelineSnapUtils::snapNearest(tick, quantizedTickLength);
    const auto splitPos = snappedTick - d->m_offset;
    m_indicator->updateIndicator(noteView, splitPos);
    m_indicator->setLastTick(tick);
}

void SplitNoteHandler::splitNoteAtPosition(NoteView *noteView, int tick) {
    if (!noteView || !d->m_clip)
        return;

    const auto note = d->m_clip->findNoteById(noteView->id());
    if (!note)
        return;

    const auto quantizedTickLength = TimelineSnapUtils::quantizeToTicks(appStatus->quantize);
    const auto snappedTick = TimelineSnapUtils::snapNearest(tick, quantizedTickLength);
    const auto splitPos = snappedTick - d->m_offset;
    const auto noteLocalStart = note->localStart();
    const auto noteLocalEnd = noteLocalStart + note->length();

    if (splitPos <= noteLocalStart || splitPos >= noteLocalEnd)
        return;

    appStatus->currentEditObject = AppStatus::EditObjectType::Note;

    const auto firstPartLength = splitPos - noteLocalStart;
    const auto secondPartLength = noteLocalEnd - splitPos;

    const auto newNote = new Note(d->m_clip);
    newNote->setLocalStart(splitPos);
    newNote->setLength(secondPartLength);
    newNote->setKeyIndex(note->keyIndex());
    newNote->setCentShift(note->centShift());
    newNote->setLanguage(note->language());
    newNote->setLyric("-");
    newNote->setPronunciation(note->pronunciation());

    clipController->onSplitNote(note->id(), newNote, firstPartLength);
    clipController->selectNotes(QList({newNote->id()}), true);

    appStatus->currentEditObject = AppStatus::EditObjectType::None;

    if (m_indicator)
        m_indicator->clearState();
}
