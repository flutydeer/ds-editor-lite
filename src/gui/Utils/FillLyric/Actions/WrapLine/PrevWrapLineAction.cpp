#include "PrevWrapLineAction.h"

namespace FillLyric {
    PrevWrapLineAction *PrevWrapLineAction::build(const QModelIndex &index, PhonicModel *model) {
        auto action = new PrevWrapLineAction;
        action->m_model = model;
        action->m_index = index;

        action->m_startIndex = index.row() * model->columnCount();
        action->m_endIndex = action->m_startIndex + model->columnCount();
        return action;
    }

    void PrevWrapLineAction::execute() {
        for (int i = m_startIndex; i < m_endIndex; i++) {
            m_model->insertWarpCell(i, Phonic());
        }
        m_model->refreshTable();
    }

    void PrevWrapLineAction::undo() {
        for (int i = m_startIndex; i < m_endIndex; i++) {
            m_model->deleteWarpCell(m_startIndex);
        }
        m_model->refreshTable();
    }
} // FillLyric