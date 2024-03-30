#include "AppendCellCmd.h"

namespace FillLyric {
    AppendCellCmd::AppendCellCmd(LyricWrapView *view, CellList *cellList, QUndoCommand *parent)
        : QUndoCommand(parent), m_view(view), m_list(cellList) {
        m_newCell = m_list->createNewCell();
    }

    void AppendCellCmd::undo() {
        m_list->removeCell(m_newCell);
    }

    void AppendCellCmd::redo() {
        m_list->appendCell(m_newCell);
    }
} // FillLyric