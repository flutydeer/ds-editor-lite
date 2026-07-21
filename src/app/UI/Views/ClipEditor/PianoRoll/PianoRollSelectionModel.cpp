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

#include <algorithm>

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

QList<NoteView *> PianoRollSelectionModel::orderedNoteItems() const {
    auto orderedItems = m_noteViews;
    std::sort(orderedItems.begin(), orderedItems.end(),
              [](const NoteView *lhs, const NoteView *rhs) {
                  if (lhs->rStart() != rhs->rStart())
                      return lhs->rStart() < rhs->rStart();
                  return lhs->id() < rhs->id();
              });
    return orderedItems;
}

void PianoRollSelectionModel::applyNoteSelection(NoteView *noteView,
                                                 const NoteSelectionMode mode) {
    if (!noteView)
        return;

    if (mode == NoteSelectionMode::Plain) {
        m_selectionAnchorId = noteView->id();
        const auto selectedItems = selectedNoteItems();
        if (selectedItems.count() <= 1 || !selectedItems.contains(noteView))
            selectOnly(noteView);
        else
            noteView->setSelected(true);
        return;
    }

    if (mode == NoteSelectionMode::Toggle) {
        m_selectionAnchorId = noteView->id();
        noteView->setSelected(!noteView->isSelected());
        return;
    }

    const auto anchor = selectionAnchor();
    if (!anchor) {
        m_selectionAnchorId = noteView->id();
        selectOnly(noteView);
        return;
    }
    selectRange(anchor, noteView, mode == NoteSelectionMode::AddRange);
}

void PianoRollSelectionModel::selectOnly(NoteView *noteView) const {
    m_view->clearNoteSelections(noteView);
    if (noteView)
        noteView->setSelected(true);
}

void PianoRollSelectionModel::clearSelectionAnchor() {
    m_selectionAnchorId = -1;
}

void PianoRollSelectionModel::invalidateSelectionAnchor(const int noteId) {
    if (m_selectionAnchorId == noteId)
        clearSelectionAnchor();
}

NoteView *PianoRollSelectionModel::selectionAnchor() const {
    return m_noteViewIndex.value(m_selectionAnchorId, nullptr);
}

void PianoRollSelectionModel::selectRange(NoteView *anchor, NoteView *target,
                                          const bool additive) const {
    const auto orderedItems = orderedNoteItems();
    const auto anchorIndex = orderedItems.indexOf(anchor);
    const auto targetIndex = orderedItems.indexOf(target);
    if (anchorIndex < 0 || targetIndex < 0)
        return;

    if (!additive)
        m_view->clearNoteSelections();
    const auto first = qMin(anchorIndex, targetIndex);
    const auto last = qMax(anchorIndex, targetIndex);
    for (auto index = first; index <= last; ++index)
        orderedItems.at(index)->setSelected(true);
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
    if (!selectionAnchor())
        clearSelectionAnchor();
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
