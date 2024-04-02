#include "ClearCellCmd.h"

namespace FillLyric {
    ClearCellCmd::ClearCellCmd(CellList *cellList, LyricCell *cell, QUndoCommand *parent)
        : QUndoCommand(parent), m_list(cellList), m_cell(cell) {
        m_index = static_cast<int>(m_list->m_cells.indexOf(cell));
        m_newCell = m_list->createNewCell();
    }

    void ClearCellCmd::undo() {
        m_list->insertCell(m_index, m_cell);
        m_list->removeCell(m_newCell);
        m_list->updateCellPos();
    }

    void ClearCellCmd::redo() {
        m_list->insertCell(m_index, m_newCell);
        m_list->removeCell(m_cell);
        m_list->updateCellPos();
    }
} // FillLyric