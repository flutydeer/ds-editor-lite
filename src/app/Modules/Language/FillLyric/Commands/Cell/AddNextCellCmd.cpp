#include "AddNextCellCmd.h"

namespace FillLyric {
    AddNextCellCmd::AddNextCellCmd(CellList *cellList, LyricCell *cell, QUndoCommand *parent)
        : QUndoCommand(parent), m_list(cellList) {
        m_index = m_list->m_cells.indexOf(cell);
        m_newCell = m_list->createNewCell();
    }

    void AddNextCellCmd::undo() {
        m_list->m_cells.remove(m_index + 1);
        m_list->sence()->removeItem(m_newCell);
        m_list->updateCellPos();
    }

    void AddNextCellCmd::redo() {
        m_list->m_cells.insert(m_index + 1, m_newCell);
        m_list->sence()->addItem(m_newCell);
        m_list->updateCellPos();
    }
} // FillLyric