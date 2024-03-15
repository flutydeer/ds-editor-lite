#include "ModelHistory.h"

#include <QDebug>

namespace FillLyric {
    void ModelHistory::undo() {
        if (m_undoStack.isEmpty())
            return;
        auto seq = m_undoStack.pop();

        seq->undo();
        m_redoStack.push(seq);
        emit undoRedoChanged(canUndo(), canRedo());
    }
    void ModelHistory::redo() {
        if (m_redoStack.isEmpty())
            return;

        auto seq = m_redoStack.pop();
        seq->execute();
        m_undoStack.push(seq);
        emit undoRedoChanged(canUndo(), canRedo());
    }
    void ModelHistory::record(MActionSequence *actions) {
        if (actions->count() <= 0)
            return;

        m_undoStack.push(actions);
        m_redoStack.clear();
        emit undoRedoChanged(canUndo(), canRedo());
    }
    void ModelHistory::reset() {
        m_undoStack.clear();
        m_redoStack.clear();
        emit undoRedoChanged(canUndo(), canRedo());
    }
    bool ModelHistory::canUndo() const {
        return !m_undoStack.isEmpty();
    }
    bool ModelHistory::canRedo() const {
        return !m_redoStack.isEmpty();
    }
} // FillLyric