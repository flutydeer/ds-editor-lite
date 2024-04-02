#include "DeleteLinesCmd.h"

namespace FillLyric {
    DeleteLinesCmd::DeleteLinesCmd(LyricWrapView *view, const QList<CellList *> &lists,
                                   QUndoCommand *parent)
        : QUndoCommand(parent), m_view(view) {
        for (const auto &list : lists) {
            m_listMap[static_cast<int>(m_view->cellLists().indexOf(list))] = list;
        }
    }

    void DeleteLinesCmd::undo() {
        for (auto it = m_listMap.begin(); it != m_listMap.end(); ++it) {
            m_view->insertList(it.key(), it.value());
        }
        m_view->repaintCellLists();
    }

    void DeleteLinesCmd::redo() {
        for (auto it = m_listMap.end(); it != m_listMap.begin();) {
            --it;
            m_view->removeList(it.value());
        }
        m_view->repaintCellLists();
    }

} // FillLyric