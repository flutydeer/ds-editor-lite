#include "DeleteCellsCmd.h"

namespace FillLyric {
    DeleteCellsCmd::DeleteCellsCmd(LyricWrapView *view, QList<LyricCell *> cells,
                                   QUndoCommand *parent)
        : QUndoCommand(parent), m_view(view), m_cells(std::move(cells)) {
        for (const auto &cell : m_cells) {
            const auto cellList = m_view->mapToList(cell->lyricRect().center().toPoint());
            if (cellList) {
                m_cellsMap[cellList][cellList->m_cells.indexOf(cell)] = cell;
            }
        }
    }

    void DeleteCellsCmd::undo() {
        const auto cellLists = m_cellsMap.keys();
        for (const auto &cellList : cellLists) {
            const auto indexes = m_cellsMap[cellList].keys();
            for (const auto &index : indexes) {
                cellList->insertCell(index, m_cellsMap[cellList][index]);
            }
        }
        m_view->repaintCellLists();
    }

    void DeleteCellsCmd::redo() {
        const auto cellLists = m_cellsMap.keys();
        for (const auto &cellList : cellLists) {
            const auto indexes = m_cellsMap[cellList].keys();
            for (const auto &index : indexes) {
                cellList->removeCell(m_cellsMap[cellList][index]);
            }
        }
        m_view->repaintCellLists();
    }
} // FillLyric