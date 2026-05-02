#include "DrawNoteHandler.h"

#include "NoteView.h"
#include "PianoRollGraphicsScene.h"
#include "PianoRollGraphicsView.h"
#include "PianoRollGraphicsView_p.h"
#include "PianoRollGraphicsViewHelper.h"
#include "PronunciationView.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
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

    const auto scenePos = q->mapToScene(event->pos());
    const auto keyIndex = d->sceneYToKeyIndexInt(scenePos.y());
    const auto tick = q->sceneXToTick(scenePos.x()) + d->m_offset;

    const auto noteView = d->noteViewAt(event->pos());
    const auto pronView = d->pronViewAt(event->pos());

    q->clearNoteSelections();
    if (noteView) {
        d->prepareForEditingNotes(event, scenePos, keyIndex, noteView);
        return false;
    } else if (pronView) {
        const auto currentNoteView = d->findNoteViewById(pronView->id());
        currentNoteView->setSelected(true);
        return false;
    } else {
        for (const auto view : d->noteViews) {
            if (view->isEditingLyric()) {
                view->finishEditingLyric();
            }
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
    const auto quantizedTickLength = TimelineSnapUtils::quantizeToTicks(appStatus->quantize);
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
    PianoRollGraphicsViewHelper::drawNote(m_currentDrawingNote->rStart(), m_currentDrawingNote->length(),
                     m_currentDrawingNote->keyIndex());
    m_drawing = false;
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
    d->restoreHandler();
}

void DrawNoteHandler::discard() {
    if (!m_drawing)
        return;
    q->scene()->removeCommonItem(m_currentDrawingNote);
    m_drawing = false;
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
    d->restoreHandler();
}

void DrawNoteHandler::prepareForDrawingNote(const int tick, const int keyIndex,
                                             const int initialLength) {
    for (const auto view : d->noteViews) {
        if (view->isEditingLyric()) {
            view->finishEditingLyric();
        }
    }

    appStatus->currentEditObject = AppStatus::EditObjectType::Note;
    const auto quantizedTickLength = TimelineSnapUtils::quantizeToTicks(appStatus->quantize);
    const auto snappedTick = TimelineSnapUtils::snapDown(tick, quantizedTickLength);
    qDebug() << "Draw note at:" << snappedTick;

    if (!m_currentDrawingNote) {
        m_currentDrawingNote = new NoteView(-1);
        m_currentDrawingNote->setSelected(true);
    }

    m_currentDrawingNote->fontPixelSize = q->property("noteFontPixelSize").toInt();
    m_currentDrawingNote->setLyric(appOptions->general()->defaultLyric);
    m_currentDrawingNote->setRStart(snappedTick - d->m_offset);
    const int length =
        initialLength >= 0 ? initialLength : TimelineSnapUtils::quantizeToTicks(appStatus->quantize);
    m_currentDrawingNote->setLength(length);
    m_currentDrawingNote->setKeyIndex(keyIndex);
    q->scene()->addCommonItem(m_currentDrawingNote);
    m_drawing = true;
}
