#include "ClearCellCmd.h"

namespace FillLyric {
    ClearCellCmd::ClearCellCmd(CellList *cellList, LyricCell *cell, QUndoCommand *parent)
        : QUndoCommand(parent), m_list(cellList), m_cell(cell) {
        m_index = m_list->m_cells.indexOf(cell);
        m_newCell = m_list->createNewCell();
    }

    void ClearCellCmd::undo() {
        m_list->m_cells.insert(m_index, m_cell);
        m_list->m_cells.remove(m_index + 1);
        m_list->sence()->addItem(m_cell);
        m_list->sence()->removeItem(m_newCell);
        m_list->updateCellPos();
    }

    void ClearCellCmd::redo() {
        m_list->m_cells.insert(m_index, m_newCell);
        m_list->m_cells.remove(m_index + 1);
        m_list->sence()->addItem(m_newCell);
        m_list->sence()->removeItem(m_cell);
        m_list->updateCellPos();
    }
} // FillLyric