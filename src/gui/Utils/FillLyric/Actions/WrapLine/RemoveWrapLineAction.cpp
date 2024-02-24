#include "RemoveWrapLineAction.h"

namespace FillLyric {
    RemoveWrapLineAction *RemoveWrapLineAction::build(const QModelIndex &index,
                                                      PhonicModel *model) {
        auto action = new RemoveWrapLineAction;
        action->m_model = model;
        action->m_index = index;

        action->m_startIndex = index.row() * model->columnCount();
        action->m_endIndex = action->m_startIndex + model->columnCount();

        QList<Phonic> tempPhonics;
        for (int i = action->m_startIndex; i < action->m_endIndex; i++) {
            tempPhonics.append(model->takeData(index.row(), i % model->columnCount()));
        }

        action->m_phonics = tempPhonics;
        return action;
    }

    void RemoveWrapLineAction::execute() {
        for (int i = m_startIndex; i < m_endIndex; i++) {
            m_model->deleteWarpCell(m_startIndex);
        }
        m_model->refreshTable();
    }

    void RemoveWrapLineAction::undo() {
        for (int i = m_startIndex; i < m_endIndex; i++) {
            m_model->insertWarpCell(i, m_phonics[i - m_startIndex]);
        }
        m_model->refreshTable();
    }
} // FillLyric