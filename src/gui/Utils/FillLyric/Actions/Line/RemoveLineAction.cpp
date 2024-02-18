#include "RemoveLineAction.h"

namespace FillLyric {

    RemoveLineAction *RemoveLineAction::build(int row, PhonicModel *model) {
        auto action = new RemoveLineAction;
        action->m_model = model;
        action->m_rmvLine = row;
        QList<Phonic> tempPhonics;
        for (int i = 0; i < model->currentLyricLength(row); i++) {
            tempPhonics.append(model->takeData(row, i));
        }
        action->m_phonics = tempPhonics;
        return action;
    }

    void RemoveLineAction::execute() {
        m_model->removeRow(m_rmvLine);
    }

    void RemoveLineAction::undo() {
        m_model->insertRow(m_rmvLine);
        for (int i = 0; i < m_phonics.size(); i++) {
            m_model->putData(m_rmvLine, i, m_phonics.at(i));
        }
    }
} // FillLyric