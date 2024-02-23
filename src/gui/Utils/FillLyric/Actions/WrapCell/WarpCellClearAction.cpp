#include "WarpCellClearAction.h"

namespace FillLyric {

    WarpCellClearAction *WarpCellClearAction::build(const QModelIndex &index, PhonicModel *model) {
        auto *action = new WarpCellClearAction();
        action->m_model = model;

        action->m_cellIndex = index.row() * model->columnCount() + index.column();
        action->m_phonic = model->takeData(index.row(), index.column());
        return action;
    }

    void WarpCellClearAction::execute() {
        int col = m_model->columnCount();

        m_model->clearData(m_cellIndex / col, m_cellIndex % col);
    }

    void WarpCellClearAction::undo() {
        int col = m_model->columnCount();

        m_model->putData(m_cellIndex / col, m_cellIndex % col, m_phonic);
    }
} // FillLyric