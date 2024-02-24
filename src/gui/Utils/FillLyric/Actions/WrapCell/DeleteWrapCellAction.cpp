#include "DeleteWrapCellAction.h"

namespace FillLyric {

    DeleteWrapCellAction *DeleteWrapCellAction::build(const QModelIndex &index,
                                                      PhonicModel *model) {
        const auto action = new DeleteWrapCellAction;
        action->m_model = model;

        action->m_cellIndex = index.row() * model->columnCount() + index.column();

        action->m_phonic = model->takeCell(index);
        return action;
    }

    void DeleteWrapCellAction::execute() {
        m_model->deleteWarpCell(m_cellIndex);
        m_model->refreshTable();
    }

    void DeleteWrapCellAction::undo() {
        int col = m_model->columnCount();
        int row = m_model->rowCount();

        if (m_cellIndex >= col * row) {
            int newRowCount = m_cellIndex / col + 1;
            for (int i = row * col; i < newRowCount * col; i++) {
                m_model->m_phonics.append(Phonic());
            }
        }
        m_model->insertWarpCell(m_cellIndex, m_phonic);
        m_model->refreshTable();
    }
} // FillLyric