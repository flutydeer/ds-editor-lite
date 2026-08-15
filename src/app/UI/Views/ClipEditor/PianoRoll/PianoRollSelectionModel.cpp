#include "PianoRollSelectionModel.h"
#include "PianoRollGraphicsView.h"
#include "NoteView.h"
#include "PronunciationView.h"
#include <lite/ProjectModel/AppModel/Note.h>
#include "Model/AppStatus/AppStatus.h"
#include "Global/AppGlobal.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include <lite/Support/Linq.h>

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

EditorSelectionUtils::PressResult
    PianoRollSelectionModel::applyNoteSelection(NoteView *noteView, const NoteSelectionMode mode) {
    if (!noteView)
        return {};

    const auto result =
        m_orderedSelection.press(selectedNoteIds(), orderedNoteIds(), noteView->id(), mode);
    applySelection(result.selection);
    return result;
}

EditorSelectionUtils::PressResult PianoRollSelectionModel::applyPressSelection(NoteView *noteView,
                                                                               const bool toggle) {
    const auto noteId = noteView ? noteView->id() : -1;
    const auto result =
        m_orderedSelection.press(selectedNoteIds(), orderedNoteIds(), noteId,
                                 toggle ? NoteSelectionMode::Toggle : NoteSelectionMode::Plain);
    applySelection(result.selection);
    return result;
}

QList<int> PianoRollSelectionModel::selectedNoteIds() const {
    QList<int> result;
    for (const auto *selected : selectedNoteItems())
        result.append(selected->id());
    return result;
}

QList<int> PianoRollSelectionModel::orderedNoteIds() const {
    QList<int> result;
    for (const auto *item : orderedNoteItems())
        result.append(item->id());
    return result;
}

void PianoRollSelectionModel::applySelection(const QList<int> &selection) const {
    for (auto *item : m_noteViews) {
        const auto selected = selection.contains(item->id());
        if (item->isSelected() != selected)
            item->setSelected(selected);
    }
}

void PianoRollSelectionModel::selectOnly(NoteView *noteView) {
    const auto noteId = noteView ? noteView->id() : -1;
    m_orderedSelection.selectOnly(noteId);
    applySelection(noteView ? QList<int>{noteId} : QList<int>());
}

void PianoRollSelectionModel::clearSelectionAnchor() {
    m_orderedSelection.clearAnchor();
}

void PianoRollSelectionModel::invalidateSelectionAnchor(const int noteId) {
    m_orderedSelection.invalidateAnchor(noteId);
}

void PianoRollSelectionModel::updateSceneSelectionState() {
    m_selectionChangeBarrier = true;
    m_view->clearNoteSelections();

    QList<NoteView *> selectedNoteViews;
    for (const auto id : appStatus->selectedNotes.get()) {
        const auto noteView = m_noteViewIndex.value(id, nullptr);
        if (!noteView) {
            // NoteView not found, skip
            continue;
        }
        noteView->setSelected(true);
        selectedNoteViews.append(noteView);
    }

    QList<int> selectedIds;
    for (const auto *noteView : selectedNoteViews)
        selectedIds.append(noteView->id());
    m_orderedSelection.synchronize(selectedIds);
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
