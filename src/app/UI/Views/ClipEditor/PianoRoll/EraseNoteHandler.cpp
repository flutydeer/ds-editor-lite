#include "EraseNoteHandler.h"

#include "NoteView.h"
#include "PianoRollGraphicsView.h"
#include "PianoRollGraphicsView_p.h"
#include "Controller/ClipController.h"
#include "Model/AppStatus/AppStatus.h"

#include <QMouseEvent>

EraseNoteHandler::EraseNoteHandler() = default;

EraseNoteHandler::~EraseNoteHandler() = default;

void EraseNoteHandler::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton)
        return;

    m_erasing = true;
    eraseNoteUnderPos(event->position().toPoint());
}

void EraseNoteHandler::mouseMoveEvent(QMouseEvent *event) {
    if (!m_erasing)
        return;
    eraseNoteUnderPos(event->position().toPoint());
}

void EraseNoteHandler::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton)
        return;
    if (!m_erasing)
        return;
    commit();
}

void EraseNoteHandler::commit() {
    if (!m_notesToErase.isEmpty()) {
        qDebug() << "Note erased count:" << m_notesToErase.count();
        clipController->onRemoveNotes(m_notesToErase);
    }
    m_notesToErase.clear();
    d->noteViewsToErase.clear();
    m_erasing = false;
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
}

void EraseNoteHandler::discard() {
    for (const auto noteView : d->noteViewsToErase)
        d->addNoteViewToScene(noteView);
    m_notesToErase.clear();
    d->noteViewsToErase.clear();
    m_erasing = false;
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
}

void EraseNoteHandler::eraseNoteUnderPos(const QPoint &pos) {
    const auto noteView = d->noteViewAt(pos);
    if (!noteView)
        return;
    appStatus->currentEditObject = AppStatus::EditObjectType::Note;
    m_notesToErase.append(noteView->id());
    d->noteViewsToErase.append(noteView);
    d->removeNoteViewFromScene(noteView);
}
