#include "DeleteCellCmd.h"

namespace FillLyric {
    DeleteCellCmd::DeleteCellCmd(CellList *cellList, LyricCell *cell, QUndoCommand *parent)
        : QUndoCommand(parent), m_list(cellList), m_cell(cell) {
        m_index = m_list->m_cells.indexOf(cell);
    }

    void DeleteCellCmd::undo() {
        m_list->m_cells.insert(m_index, m_cell);
        m_list->sence()->addItem(m_cell);
        m_list->updateCellPos();
    }

    void DeleteCellCmd::redo() {
        m_list->m_cells.remove(m_index);
        m_list->sence()->removeItem(m_cell);
        m_list->updateCellPos();
    }
} // FillLyric