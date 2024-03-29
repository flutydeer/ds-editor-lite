#include "AddNextLineCmd.h"

namespace FillLyric {
    AddNextLineCmd::AddNextLineCmd(LyricWrapView *view, CellList *cellList, QUndoCommand *parent)
        : QUndoCommand(parent), m_view(view), m_newList(cellList) {
        m_index = m_view->m_cellLists.indexOf(cellList);
        m_newList = m_view->createNewList();

        for (const auto &cell : m_newList->m_cells) {
            m_newList->sence()->removeItem(cell);
        }
    }

    void AddNextLineCmd::undo() {
        m_view->m_cellLists.remove(m_index + 1);
        for (const auto &cell : m_newList->m_cells) {
            m_newList->sence()->removeItem(cell);
        }
        m_view->repaintCellLists();
    }

    void AddNextLineCmd::redo() {
        m_view->m_cellLists.insert(m_index + 1, m_newList);
        for (const auto &cell : m_newList->m_cells) {
            m_newList->sence()->addItem(cell);
        }
        m_view->repaintCellLists();
    }
} // FillLyric