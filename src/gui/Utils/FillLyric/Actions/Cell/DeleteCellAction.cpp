#include "DeleteCellAction.h"

namespace FillLyric {

    DeleteCellAction *DeleteCellAction::build(const QModelIndex &index, PhonicModel *model) {
        auto action = new DeleteCellAction;
        action->m_model = model;
        action->m_index = index;
        action->m_phonic = model->cellTake(index);

        int row = index.row();
        QList<moveInfo> tempMoveList;
        for (int i = index.column() + 1; i <= model->columnCount(); i++) {
            tempMoveList.append({row, i, row, i - 1});
        }
        action->m_moveList = tempMoveList;
        return action;
    }

    void DeleteCellAction::execute() {
        moveExecute(m_moveList, m_model);
    }

    void DeleteCellAction::undo() {
        moveUndo(m_moveList, m_model);
        m_model->cellPut(m_index, m_phonic);
        m_model->shrinkModel();
    }
} // FillLyric