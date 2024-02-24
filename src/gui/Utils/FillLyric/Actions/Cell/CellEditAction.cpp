#include "CellEditAction.h"

namespace FillLyric {

    CellEditAction *CellEditAction::build(const QModelIndex &index, PhonicModel *model,
                                          const QList<Phonic> &oldPhonics,
                                          const QList<Phonic> &newPhonics) {
        const auto action = new CellEditAction();
        action->m_model = model;
        action->m_index = index;
        action->m_oldPhonics = oldPhonics;
        action->m_newPhonics = newPhonics;
        return action;
    }

    void CellEditAction::execute() {
        const int row = m_index.row();
        for (int i = 0; i < m_oldPhonics.size(); ++i) {
            m_model->putData(row, i, m_newPhonics[i]);
        }
    }

    void CellEditAction::undo() {
        const int row = m_index.row();
        for (int i = 0; i < m_oldPhonics.size(); ++i) {
            m_model->putData(row, i, m_oldPhonics[i]);
        }
    }

} // FillLyric