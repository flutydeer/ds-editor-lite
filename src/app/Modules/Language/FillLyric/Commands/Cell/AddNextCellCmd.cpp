#include "AddNextCellCmd.h"

namespace FillLyric {
    AddNextCellCmd::AddNextCellCmd(CellList *cellList, LyricCell *cell, QUndoCommand *parent)
        : QUndoCommand(parent), m_list(cellList) {
        m_index = static_cast<int>(m_list->m_cells.indexOf(cell));
        m_newCell = m_list->createNewCell();
    }

    void AddNextCellCmd::undo() {
        m_list->removeCell(m_newCell);
        m_list->updateCellPos();
    }

    void AddNextCellCmd::redo() {
        m_list->insertCell(m_index + 1, m_newCell);
        m_list->updateCellPos();
    }
} // FillLyric