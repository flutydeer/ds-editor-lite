#include "ClearCellsCmd.h"

#include <utility>

namespace FillLyric {
    ClearCellsCmd::ClearCellsCmd(LyricWrapView *view, QList<LyricCell *> cells,
                                 QUndoCommand *parent)
        : QUndoCommand(parent), m_view(view), m_cells(std::move(cells)) {
        for (const auto &cell : m_cells) {
            if (const auto cellList = m_view->mapToList(cell->lyricRect().center().toPoint())) {
                m_cellsMap[cellList][static_cast<int>(cellList->m_cells.indexOf(cell))] = {
                    cellList->createNewCell(), cell};
            }
        }
    }

    void ClearCellsCmd::undo() {
        const auto cellLists = m_cellsMap.keys();
        for (const auto &cellList : cellLists) {
            const auto indexes = m_cellsMap[cellList].keys();
            for (const auto &index : indexes) {
                const auto &cell = m_cellsMap[cellList][index];
                cellList->insertCell(index, cell.m_old);
                cellList->removeCell(cell.m_new);
            }
        }
        m_view->repaintCellLists();
    }

    void ClearCellsCmd::redo() {
        const auto cellLists = m_cellsMap.keys();
        for (const auto &cellList : cellLists) {
            const auto indexes = m_cellsMap[cellList].keys();
            for (const auto &index : indexes) {
                const auto &cell = m_cellsMap[cellList][index];
                cellList->insertCell(index, cell.m_new);
                cellList->removeCell(cell.m_old);
            }
        }
        m_view->repaintCellLists();
    }
} // FillLyric