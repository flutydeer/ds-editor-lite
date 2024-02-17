#include "CellMoveAction.h"

namespace FillLyric {
    CellMoveAction *CellMoveAction::build(const QModelIndex &source, const QModelIndex &target,
                                          PhonicModel *model) {
        auto action = new CellMoveAction;
        action->m_model = model;
        action->m_source = source;
        action->m_target = target;
        return action;
    }

    void CellMoveAction::execute() {
        m_targetPhonic = m_model->cellTake(m_target);
        m_model->cellMove(m_source, m_target);
    }

    void CellMoveAction::undo() {
        m_model->cellMove(m_target, m_source);
        m_model->cellPut(m_target, m_targetPhonic);
    }
} // FillLyric