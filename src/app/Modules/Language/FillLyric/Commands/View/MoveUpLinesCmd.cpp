#include "MoveUpLinesCmd.h"

namespace FillLyric {
    MoveUpLinesCmd::MoveUpLinesCmd(LyricWrapView *view, const QList<CellList *> &lists,
                                   QUndoCommand *parent)
        : QUndoCommand(parent), m_view(view) {
        QMap<qlonglong, CellList *> map;
        for (const auto &list : lists)
            map[m_view->cellLists().indexOf(list)] = list;
        m_lists = map.values();
    }

    void MoveUpLinesCmd::undo() {
        m_view->moveDownLists(m_lists);
        m_view->repaintCellLists();
    }

    void MoveUpLinesCmd::redo() {
        m_view->moveUpLists(m_lists);
        m_view->repaintCellLists();
    }
} // FillLyric