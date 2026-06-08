#include "EraseNoteHandler.h"

#include "NoteView.h"
#include "PianoRollGraphicsView.h"
#include "PianoRollGraphicsView_p.h"
#include "Controller/ClipController.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/EditSessionManager.h"

#include <QMouseEvent>

EraseNoteHandler::EraseNoteHandler() = default;

EraseNoteHandler::~EraseNoteHandler() = default;

bool EraseNoteHandler::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton)
        return true;

    m_erasing = true;
    eraseNoteUnderPos(event->position().toPoint());
    return true;
}

bool EraseNoteHandler::mouseMoveEvent(QMouseEvent *event) {
    if (!m_erasing)
        return true;
    eraseNoteUnderPos(event->position().toPoint());
    return true;
}

bool EraseNoteHandler::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton)
        return true;
    if (!m_erasing)
        return true;
    commit();
    return true;
}

void EraseNoteHandler::commit() {
    if (!m_notesToErase.isEmpty()) {
        qDebug() << "Note erased count:" << m_notesToErase.count();
        clipController->onRemoveNotes(m_notesToErase);
    }
    m_notesToErase.clear();
    d->noteViewsToErase.clear();
    m_erasing = false;
    editSessionManager->endActiveTransaction(EditSessionEndReason::Commit);
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
}

void EraseNoteHandler::discard() {
    for (const auto noteView : d->noteViewsToErase)
        d->addNoteViewToScene(noteView);
    m_notesToErase.clear();
    d->noteViewsToErase.clear();
    m_erasing = false;
    editSessionManager->endActiveTransaction(EditSessionEndReason::Discard);
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
}

void EraseNoteHandler::eraseNoteUnderPos(const QPoint &pos) {
    const auto noteView = d->noteViewAt(pos);
    if (!noteView)
        return;
    if (!editSessionManager->hasActiveTransaction())
        editSessionManager->beginTransaction(AppStatus::EditObjectType::Note,
                                             d->m_clip ? d->m_clip->id() : -1, {}, {}, {}, {},
                                             true);
    appStatus->currentEditObject = AppStatus::EditObjectType::Note;
    if (!m_notesToErase.contains(noteView->id()))
        m_notesToErase.append(noteView->id());
    editSessionManager->addNoteIds({noteView->id()});
    d->noteViewsToErase.append(noteView);
    d->removeNoteViewFromScene(noteView);
}
