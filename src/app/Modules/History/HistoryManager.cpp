//
// Created by fluty on 2024/2/7.
//

#include "HistoryManager.h"

#include <QDebug>

void HistoryManager::undo() {
    if (m_undoStack.isEmpty())
        return;

    auto seq = m_undoStack.pop();
    seq->undo();
    m_redoStack.push(seq);

    emit undoRedoChanged(canUndo(), undoActionName(), canRedo(), redoActionName());
}
void HistoryManager::redo() {
    if (m_redoStack.isEmpty())
        return;

    auto seq = m_redoStack.pop();
    seq->execute();
    m_undoStack.push(seq);


    emit undoRedoChanged(canUndo(), undoActionName(), canRedo(), redoActionName());
}
void HistoryManager::record(ActionSequence *actions) {
    if (actions->count() <= 0)
        return;

    m_undoStack.push(actions);
    m_redoStack.clear();
    emit undoRedoChanged(canUndo(), undoActionName(), canRedo(), redoActionName());
}
void HistoryManager::reset() {
    qDebug() << "HistoryManager::reset()";
    for (auto seq : m_undoStack)
        delete seq;
    for (auto seq : m_redoStack)
        delete seq;
    m_undoStack.clear();
    m_redoStack.clear();
    m_savePoint = nullptr;
    m_isSavePointSet = false;
    emit undoRedoChanged(canUndo(), "", canRedo(), "");
}
bool HistoryManager::isOnSavePoint() const {
    auto flag = false;
    if (m_undoStack.isEmpty() && m_redoStack.isEmpty())
        return true;

    if (!m_isSavePointSet)
        return flag;

    if (m_savePoint != nullptr) {
        if (!m_undoStack.isEmpty())
            if (m_undoStack.top() == m_savePoint)
                flag = true;
    } else if (m_undoStack.isEmpty())
        flag = true;
    return flag;
}
void HistoryManager::setSavePoint() {
    m_isSavePointSet = true;
    if (m_undoStack.isEmpty()) {
        m_savePoint = nullptr;
        return;
    }
    m_savePoint = m_undoStack.top();
}
bool HistoryManager::canUndo() const {
    return !m_undoStack.isEmpty();
}
bool HistoryManager::canRedo() const {
    return !m_redoStack.isEmpty();
}
QString HistoryManager::undoActionName() const {
    return m_undoStack.isEmpty() ? "" : m_undoStack.top()->name();
}
QString HistoryManager::redoActionName() const {
    return m_redoStack.isEmpty() ? "" : m_redoStack.top()->name();
}