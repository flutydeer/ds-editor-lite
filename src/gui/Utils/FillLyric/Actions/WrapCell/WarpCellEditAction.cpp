#include "WarpCellEditAction.h"

namespace FillLyric {

    WarpCellEditAction *WarpCellEditAction::build(const QModelIndex &index, PhonicModel *model,
                                                  const QList<Phonic> &oldPhonics,
                                                  const QList<Phonic> &newPhonics) {
        auto *action = new WarpCellEditAction();
        action->m_model = model;

        action->m_startIndex = index.row() * model->columnCount();
        action->m_endIndex = action->m_startIndex + model->columnCount() - 1;

        action->m_oldPhonics = oldPhonics;
        action->m_newPhonics = newPhonics;
        return action;
    }

    void WarpCellEditAction::execute() {
        int columnCount = m_model->columnCount();

        for (int i = m_startIndex; i <= m_endIndex; ++i) {
            m_model->putData(i / columnCount, i % columnCount, m_newPhonics[i - m_startIndex]);
        }
    }

    void WarpCellEditAction::undo() {
        int columnCount = m_model->columnCount();

        for (int i = m_startIndex; i <= m_endIndex; ++i) {
            m_model->putData(i / columnCount, i % columnCount, m_oldPhonics[i - m_startIndex]);
        }
    }
} // FillLyric