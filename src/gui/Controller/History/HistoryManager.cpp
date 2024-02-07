//
// Created by fluty on 2024/2/7.
//

#include "HistoryManager.h"
void HistoryManager::undo() {
    if (m_undoStack.isEmpty())
        return;

    auto seq = m_undoStack.pop();
    seq->undo();
    m_redoStack.push(seq);
    emit undoRedoChanged();
}
void HistoryManager::redo() {
    if (m_redoStack.isEmpty())
        return;

    auto seq = m_redoStack.pop();
    seq->execute();
    m_undoStack.push(seq);
    emit undoRedoChanged();
}
void HistoryManager::record(ActionSequence *actions) {
    if (actions->count() <= 0)
        return;

    m_undoStack.push(actions);
    m_redoStack.clear();
    emit undoRedoChanged();
}
bool HistoryManager::canUndo() {
    return !m_undoStack.isEmpty();
}
bool HistoryManager::canRedo() {
    return !m_redoStack.isEmpty();
}