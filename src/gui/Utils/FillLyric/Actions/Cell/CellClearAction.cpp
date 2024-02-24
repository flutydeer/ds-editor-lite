#include "CellClearAction.h"

namespace FillLyric {
    CellClearAction *CellClearAction::build(const QModelIndex &index, PhonicModel *model) {
        auto action = new CellClearAction;
        action->m_model = model;
        action->m_index = index;
        action->m_phonic = model->takeCell(index);
        return action;
    }

    void CellClearAction::execute() {
        m_model->clearCell(m_index);
    }

    void CellClearAction::undo() {
        m_model->putCell(m_index, m_phonic);
    }
} // FillLyric