#include "AddNextLineCmd.h"

namespace FillLyric {
    AddNextLineCmd::AddNextLineCmd(LyricWrapView *view, CellList *cellList, QUndoCommand *parent)
        : QUndoCommand(parent), m_view(view), m_newList(cellList) {
        m_index = static_cast<int>(m_view->cellLists().indexOf(cellList));
        m_newList = m_view->createNewList();
        m_newList->removeFromScene();
    }

    void AddNextLineCmd::undo() {
        m_view->removeList(m_index + 1);
    }

    void AddNextLineCmd::redo() {
        m_view->insertList(m_index + 1, m_newList);
    }
} // FillLyric