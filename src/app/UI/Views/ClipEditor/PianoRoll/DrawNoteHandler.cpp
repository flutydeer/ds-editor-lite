#include "DrawNoteHandler.h"

#include "NoteView.h"
#include "PianoRollGraphicsScene.h"
#include "PianoRollGraphicsView.h"
#include "PianoRollGraphicsView_p.h"
#include "PianoRollCoord.h"
#include "NoteInteractionController.h"
#include "PianoRollGraphicsViewHelper.h"
#include "PronunciationView.h"
#include "Controller/ClipController.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/EditSessionManager.h"
#include "Utils/TimelineSnapUtils.h"

#include <QDebug>
#include <QMouseEvent>

DrawNoteHandler::DrawNoteHandler() = default;

DrawNoteHandler::~DrawNoteHandler() {
    delete m_currentDrawingNote;
}

bool DrawNoteHandler::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton)
        return false;
    d->finishEditingLyric();

    const auto scenePos = q->mapToScene(event->pos());
    const auto keyIndex =
        PianoRollCoord::sceneYToKeyIndexInt(scenePos.y(), q->scaleY() * noteHeight);
    const auto tick = q->sceneXToTick(scenePos.x()) + d->m_offset;

    const auto noteView = d->noteViewAt(event->pos());
    const auto pronView = d->pronViewAt(event->pos());

    q->clearNoteSelections();
    if (noteView) {
        d->m_interactionController->prepareForEditingNotes(event, scenePos, keyIndex, noteView);
        return false;
    } else if (pronView) {
        const auto currentNoteView = d->findNoteViewById(pronView->id());
        currentNoteView->setSelected(true);
        return false;
    } else {
        for (const auto view : d->noteViews) {
            if (view->pronunciationView() && view->pronunciationView()->isEditingPronunciation()) {
                view->pronunciationView()->finishEditingPronunciation();
            }
        }
        prepareForDrawingNote(tick, keyIndex);
        return true;
    }
}

bool DrawNoteHandler::mouseMoveEvent(QMouseEvent *event) {
    if (!m_drawing)
        return false;

    const auto scenePos = q->mapToScene(event->pos());
    const auto tick = q->sceneXToTick(scenePos.x()) + d->m_offset;
    const auto quantizedTickLength =
        TimelineSnapUtils::quantizeToTicks(appStatus->pianoRollQuantize);
    const auto snappedTick = TimelineSnapUtils::snapDown(tick, quantizedTickLength);
    const auto targetLength = snappedTick - d->m_offset - m_currentDrawingNote->rStart();
    if (targetLength >= quantizedTickLength)
        m_currentDrawingNote->setLength(targetLength);
    return true;
}

bool DrawNoteHandler::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton)
        return false;
    if (!m_drawing)
        return false;
    commit();
    return true;
}

void DrawNoteHandler::commit() {
    if (!m_drawing)
        return;
    d->removeNoteViewFromScene(m_currentDrawingNote);
    PianoRollGraphicsViewHelper::drawNote(m_currentDrawingNote->rStart(),
                                          m_currentDrawingNote->length(),
                                          m_currentDrawingNote->keyIndex());
    m_drawing = false;
    editSessionManager->endActiveTransaction(EditSessionEndReason::Commit);
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
    d->restoreHandler();
}

void DrawNoteHandler::discard() {
    if (!m_drawing)
        return;
    q->scene()->removeCommonItem(m_currentDrawingNote);
    m_drawing = false;
    editSessionManager->endActiveTransaction(EditSessionEndReason::Discard);
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
    d->restoreHandler();
}

void DrawNoteHandler::prepareForDrawingNote(const int tick, const int keyIndex,
                                            const int initialLength) {
    d->finishEditingLyric();

    const auto quantizedTickLength =
        TimelineSnapUtils::quantizeToTicks(appStatus->pianoRollQuantize);
    const auto snappedTick = TimelineSnapUtils::snapDown(tick, quantizedTickLength);
    qDebug() << "Draw note at:" << snappedTick;

    if (!m_currentDrawingNote) {
        m_currentDrawingNote = new NoteView(-1);
        m_currentDrawingNote->setSelected(true);
    }

    m_currentDrawingNote->fontPixelSize = q->property("noteFontPixelSize").toInt();
    const auto singingClip = dynamic_cast<SingingClip *>(clipController->clip());
    editSessionManager->beginTransaction(AppStatus::EditObjectType::Note,
                                         singingClip ? singingClip->id() : -1, {}, {}, {}, {},
                                         true);
    appStatus->currentEditObject = AppStatus::EditObjectType::Note;
    const auto language = singingClip ? singingClip->defaultLanguage()
                                      : appOptions->general()->defaultSingingLanguage;
    m_currentDrawingNote->setLyric(appOptions->general()->defaultLyricForLanguage(language));
    m_currentDrawingNote->setRStart(snappedTick - d->m_offset);
    const int length = initialLength >= 0
                           ? initialLength
                           : TimelineSnapUtils::quantizeToTicks(appStatus->pianoRollQuantize);
    m_currentDrawingNote->setLength(length);
    m_currentDrawingNote->setKeyIndex(keyIndex);
    q->scene()->addCommonItem(m_currentDrawingNote);
    m_drawing = true;
}
