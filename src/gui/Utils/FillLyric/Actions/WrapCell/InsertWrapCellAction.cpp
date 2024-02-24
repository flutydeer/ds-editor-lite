#include "InsertWrapCellAction.h"

namespace FillLyric {
    InsertWrapCellAction *InsertWrapCellAction::build(const QModelIndex &index,
                                                      PhonicModel *model) {
        auto action = new InsertWrapCellAction;
        action->m_model = model;

        action->m_cellIndex = index.row() * model->columnCount() + index.column();
        return action;
    }

    void InsertWrapCellAction::execute() {
        m_model->insertWarpCell(m_cellIndex, Phonic());
        m_model->refreshTable();
    }

    void InsertWrapCellAction::undo() {
        m_model->deleteWarpCell(m_cellIndex);
        m_model->refreshTable();
    }
} // FillLyric