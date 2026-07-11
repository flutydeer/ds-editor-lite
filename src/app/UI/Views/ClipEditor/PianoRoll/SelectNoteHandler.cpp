#include "SelectNoteHandler.h"

#include "NoteView.h"
#include "PianoRollGraphicsView.h"
#include "PianoRollGraphicsView_p.h"
#include "PianoRollCoord.h"
#include "NoteInteractionController.h"
#include "PronunciationView.h"

#include <QMouseEvent>

SelectNoteHandler::SelectNoteHandler() = default;

SelectNoteHandler::~SelectNoteHandler() = default;

bool SelectNoteHandler::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton)
        return false;
    d->finishEditingLyric();

    const auto scenePos = q->mapToScene(event->pos());
    const auto keyIndex =
        PianoRollCoord::sceneYToKeyIndexInt(scenePos.y(), q->scaleY() * noteHeight);
    const auto noteView = d->noteViewAt(event->pos());

    if (noteView) {
        d->m_interactionController->prepareForEditingNotes(event, scenePos, keyIndex, noteView);
    } else {
        for (const auto view : d->noteViews) {
            if (view->pronunciationView() && view->pronunciationView()->isEditingPronunciation()) {
                view->pronunciationView()->finishEditingPronunciation();
            }
        }
    }
    return false;
}

bool SelectNoteHandler::mouseMoveEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    return false;
}

bool SelectNoteHandler::mouseReleaseEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    return false;
}
