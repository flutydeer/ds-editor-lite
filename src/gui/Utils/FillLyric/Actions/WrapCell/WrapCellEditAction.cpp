#include "WrapCellEditAction.h"

namespace FillLyric {

    WrapCellEditAction *WrapCellEditAction::build(const QModelIndex &index, PhonicModel *model,
                                                  const Phonic &newPhonic) {
        auto *action = new WrapCellEditAction();
        action->m_model = model;

        action->m_cellIndex = index.row() * model->columnCount() + index.column();

        action->m_oldPhonic = model->takeCell(index);
        action->m_newPhonic = newPhonic;
        return action;
    }

    void WrapCellEditAction::execute() {
        const int col = m_model->columnCount();
        const int row = m_model->rowCount();

        if (m_cellIndex >= col * row) {
            const int newRowCount = m_cellIndex / col + 1;
            for (int i = row * col; i < newRowCount * col; i++) {
                m_model->m_phonics.append(Phonic());
            }
        }
        m_model->editWarpCell(m_cellIndex, m_newPhonic);
        m_model->refreshTable();
    }

    void WrapCellEditAction::undo() {
        m_model->editWarpCell(m_cellIndex, m_oldPhonic);
        m_model->refreshTable();
    }
} // FillLyric