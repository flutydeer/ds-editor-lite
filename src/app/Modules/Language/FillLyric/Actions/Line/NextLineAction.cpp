#include "NextLineAction.h"

namespace FillLyric {

    NextLineAction *NextLineAction::build(const int &row, PhonicModel *model) {
        const auto action = new NextLineAction;
        action->m_model = model;
        action->m_newLine = row + 1;
        return action;
    }

    void NextLineAction::execute() {
        m_model->insertRow(m_newLine);
    }

    void NextLineAction::undo() {
        m_model->removeRow(m_newLine);
    }
} // FillLyric