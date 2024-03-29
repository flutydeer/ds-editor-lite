#include "DeleteLineCmd.h"

namespace FillLyric {
    DeleteLineCmd::DeleteLineCmd(LyricWrapView *view, CellList *cellList, QUndoCommand *parent)
        : QUndoCommand(parent), m_view(view), m_list(cellList) {
        m_index = static_cast<int>(m_view->cellLists().indexOf(cellList));
    }

    void DeleteLineCmd::undo() {
        m_view->insertList(m_index, m_list);
    }

    void DeleteLineCmd::redo() {
        m_view->removeList(m_index);
    }
} // FillLyric