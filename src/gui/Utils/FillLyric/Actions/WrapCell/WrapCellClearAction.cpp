#include "WrapCellClearAction.h"

namespace FillLyric {

    WrapCellClearAction *WrapCellClearAction::build(const QModelIndex &index, PhonicModel *model) {
        auto *action = new WrapCellClearAction();
        action->m_model = model;
        action->m_index = index;

        action->m_cellIndex = index.row() * model->columnCount() + index.column();
        action->m_phonic = model->takeData(index.row(), index.column());
        return action;
    }

    void WrapCellClearAction::execute() {
        int col = m_model->columnCount();
        int row = m_model->rowCount();

        if (m_cellIndex >= col * row) {
            int newRowCount = m_cellIndex / col + 1;
            for (int i = row * col; i < newRowCount * col; i++) {
                m_model->m_phonics.append(Phonic());
            }
        }
        m_model->editWarpCell(m_cellIndex, Phonic());
        m_model->refreshTable();
    }

    void WrapCellClearAction::undo() {
        m_model->editWarpCell(m_cellIndex, m_phonic);
        m_model->refreshTable();
    }
} // FillLyric