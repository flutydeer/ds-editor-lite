#include "AddPrevLineCmd.h"

namespace FillLyric {
    AddPrevLineCmd::AddPrevLineCmd(LyricWrapView *view, CellList *cellList, QUndoCommand *parent)
        : QUndoCommand(parent), m_view(view), m_newList(cellList) {
        m_index = m_view->m_cellLists.indexOf(cellList);
        m_newList = m_view->createNewList();

        for (const auto &cell : m_newList->m_cells) {
            m_newList->sence()->removeItem(cell);
        }
    }

    void AddPrevLineCmd::undo() {
        m_view->m_cellLists.remove(m_index);
        for (const auto &cell : m_newList->m_cells) {
            m_newList->sence()->removeItem(cell);
        }
        m_view->repaintCellLists();
    }

    void AddPrevLineCmd::redo() {
        m_view->m_cellLists.insert(m_index, m_newList);
        for (const auto &cell : m_newList->m_cells) {
            m_newList->sence()->addItem(cell);
        }
        m_view->repaintCellLists();
    }
} // FillLyric