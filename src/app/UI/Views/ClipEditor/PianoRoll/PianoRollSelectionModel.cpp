#include "PianoRollSelectionModel.h"
#include "PianoRollGraphicsView.h"
#include "NoteView.h"
#include "PronunciationView.h"
#include "Model/AppModel/Note.h"
#include "Model/AppStatus/AppStatus.h"
#include "Global/AppGlobal.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "Utils/Linq.h"

#include <QDebug>

#include "UI/Views/Common/TimeGraphicsScene.h"

PianoRollSelectionModel::PianoRollSelectionModel(PianoRollGraphicsView *view,
                                                 QList<NoteView *> &noteViews,
                                                 QHash<int, NoteView *> &noteViewIndex,
                                                 QList<Note *> &notes, QObject *parent)
    : QObject(parent), m_noteViews(noteViews), m_noteViewIndex(noteViewIndex), m_notes(notes),
      m_view(view) {
}

QList<NoteView *> PianoRollSelectionModel::selectedNoteItems() const {
    return Linq::where(m_noteViews, L_PRED(n, n->isSelected()));
}

void PianoRollSelectionModel::updateSceneSelectionState() {
    m_selectionChangeBarrier = true;
    m_view->clearNoteSelections();

    for (const auto id : appStatus->selectedNotes.get()) {
        const auto noteView = m_noteViewIndex.value(id, nullptr);
        if (!noteView) {
            // NoteView not found, skip
            continue;
        }
        noteView->setSelected(true);
    }
    m_selectionChangeBarrier = false;
}

void PianoRollSelectionModel::updateOverlappedState() {
    for (const auto note : m_notes) {
        const auto noteView = m_noteViewIndex.value(note->id(), nullptr);
        if (!noteView) {
            continue;
        }
        noteView->setOverlapped(note->overlapped());
    }
    m_view->update();
}

void PianoRollSelectionModel::clearPastePreviewViews() {
    for (auto view : m_pastePreviewViews) {
        if (view->scene() == reinterpret_cast<QGraphicsScene *>(m_view->scene())) {
            m_view->scene()->removeCommonItem(view);
            if (view->pronunciationView())
                m_view->scene()->removeCommonItem(view->pronunciationView());
        }
        delete view;
    }
    m_pastePreviewViews.clear();
}