#include "LineInsertAction.h"

namespace FillLyric {

    LineInsertAction *LineInsertAction::build(int row, PhonicModel *model) {
        auto action = new LineInsertAction;
        action->m_model = model;
        action->m_row = row;
        return action;
    }

    void LineInsertAction::execute() {
        m_model->insertRow(m_row);
    }

    void LineInsertAction::undo() {
        m_model->removeRow(m_row);
    }
} // FillLyric