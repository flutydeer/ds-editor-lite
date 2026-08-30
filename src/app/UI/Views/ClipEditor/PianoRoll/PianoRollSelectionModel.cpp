#include "PianoRollSelectionModel.h"
#include "PianoRollGraphicsView.h"
#include "NoteView.h"
#include "PronunciationView.h"
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include "Model/AppStatus/AppStatus.h"
#include "Global/AppGlobal.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include <QDebug>

#include "UI/Views/Common/TimeGraphicsScene.h"

PianoRollSelectionModel::PianoRollSelectionModel(PianoRollGraphicsView *view,
                                                 QList<NoteView *> &noteViews,
                                                 QHash<int, NoteView *> &noteViewIndex,
                                                 QObject *parent)
    : QObject(parent), m_noteViews(noteViews), m_noteViewIndex(noteViewIndex), m_view(view) {
}

QList<NoteView *> PianoRollSelectionModel::selectedNoteItems() const {
    QList<NoteView *> result;
    for (auto *item : orderedNoteItems()) {
        if (item->isSelected())
            result.append(item);
    }
    return result;
}

QList<NoteView *> PianoRollSelectionModel::orderedNoteItems() const {
    QList<NoteView *> result;
    if (!m_clip)
        return result;
    result.reserve(m_clip->notes().count());
    for (const auto *note : m_clip->notes()) {
        if (auto *item = m_noteViewIndex.value(note->id(), nullptr))
            result.append(item);
    }
    return result;
}

void PianoRollSelectionModel::setDataContext(SingingClip *clip) {
    m_clip = clip;
}

EditorSelectionUtils::PressResult
    PianoRollSelectionModel::applyNoteSelection(NoteView *noteView,
                                                const Qt::KeyboardModifiers modifiers) {
    if (!noteView)
        return {};

    const auto result =
        m_orderedSelection.press(selectedNoteIds(), orderedNoteIds(), noteView->id(), modifiers);
    applySelection(result.selection);
    return result;
}

EditorSelectionUtils::PressResult PianoRollSelectionModel::applyPressSelection(NoteView *noteView,
                                                                               const bool toggle) {
    const auto noteId = noteView ? noteView->id() : -1;
    m_orderedSelection.cancelPress();
    const auto result = m_orderedSelection.applyPress(selectedNoteIds(), orderedNoteIds(), noteId,
                                                      toggle ? NoteSelectionMode::Toggle
                                                             : NoteSelectionMode::Plain);
    applySelection(result.selection);
    return result;
}

void PianoRollSelectionModel::finalizePressSelection(const bool pointerMoved) {
    applySelection(m_orderedSelection.release(selectedNoteIds(), pointerMoved));
}

void PianoRollSelectionModel::cancelPressSelection() {
    m_orderedSelection.cancelPress();
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
    m_orderedSelection.cancelPress();
    m_orderedSelection.selectOnly(noteId);
    applySelection(noteView ? QList<int>{noteId} : QList<int>());
}

void PianoRollSelectionModel::clearSelectionAnchor() {
    m_orderedSelection.clearAnchor();
    m_orderedSelection.cancelPress();
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
    if (!m_clip)
        return;
    for (const auto note : m_clip->notes()) {
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
