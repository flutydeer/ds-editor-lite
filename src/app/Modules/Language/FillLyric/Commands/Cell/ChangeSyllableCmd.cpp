#include "ChangeSyllableCmd.h"

namespace FillLyric {
    ChangeSyllableCmd::ChangeSyllableCmd(CellList *cellList, LyricCell *cell,
                                         const QString &syllableRevised, QUndoCommand *parent)
        : QUndoCommand(parent), m_list(cellList), m_cell(cell) {
        m_index = m_list->m_cells.indexOf(cell);
        m_syllableRevised = syllableRevised;
    }

    void ChangeSyllableCmd::undo() {
        m_cell->note()->syllableRevised = QString();
        m_cell->note()->revised = false;
        m_list->updateRect(m_cell);
    }

    void ChangeSyllableCmd::redo() {
        m_cell->note()->syllableRevised = m_syllableRevised;
        m_cell->note()->revised = true;
        m_list->updateRect(m_cell);
    }
} // Lyric