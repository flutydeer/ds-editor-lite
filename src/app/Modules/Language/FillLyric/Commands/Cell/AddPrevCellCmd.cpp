#include "AddPrevCellCmd.h"

namespace FillLyric {
    AddPrevCellCmd::AddPrevCellCmd(CellList *cellList, LyricCell *cell, QUndoCommand *parent)
        : QUndoCommand(parent), m_list(cellList) {
        m_index = m_list->m_cells.indexOf(cell);
        m_newCell = m_list->createNewCell();
    }

    void AddPrevCellCmd::undo() {
        m_list->m_cells.remove(m_index);
        m_list->scene()->removeItem(m_newCell);
        m_list->updateCellPos();
    }

    void AddPrevCellCmd::redo() {
        m_list->m_cells.insert(m_index, m_newCell);
        m_list->scene()->addItem(m_newCell);
        m_list->updateCellPos();
    }
} // FillLyric