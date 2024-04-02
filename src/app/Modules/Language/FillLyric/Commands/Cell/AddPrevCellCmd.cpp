#include "AddPrevCellCmd.h"

namespace FillLyric {
    AddPrevCellCmd::AddPrevCellCmd(CellList *cellList, LyricCell *cell, QUndoCommand *parent)
        : QUndoCommand(parent), m_list(cellList) {
        m_index = static_cast<int>(m_list->m_cells.indexOf(cell));
        m_newCell = m_list->createNewCell();
    }

    void AddPrevCellCmd::undo() {
        m_list->removeCell(m_newCell);
        m_list->updateCellPos();
    }

    void AddPrevCellCmd::redo() {
        m_list->insertCell(m_index, m_newCell);
        m_list->updateCellPos();
    }
} // FillLyric