#include "DeleteLineCmd.h"

namespace FillLyric {
    DeleteLineCmd::DeleteLineCmd(LyricWrapView *view, CellList *cellList, QUndoCommand *parent)
        : QUndoCommand(parent), m_view(view), m_list(cellList) {
        m_index = m_view->m_cellLists.indexOf(cellList);
    }

    void DeleteLineCmd::undo() {
        m_view->m_cellLists.insert(m_index, m_list);
        for (const auto &cell : m_list->m_cells) {
            m_list->sence()->addItem(cell);
        }
        m_view->repaintCellLists();
    }

    void DeleteLineCmd::redo() {
        m_view->m_cellLists.remove(m_index);
        for (const auto &cell : m_list->m_cells) {
            m_list->sence()->removeItem(cell);
        }
        m_view->repaintCellLists();
    }
} // FillLyric