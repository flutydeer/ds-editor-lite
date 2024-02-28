#include "ModelShrinkAction.h"

namespace FillLyric {

    ModelShrinkAction *ModelShrinkAction::build(PhonicModel *model) {
        const auto action = new ModelShrinkAction;
        action->m_model = model;
        action->m_rawColCount = model->columnCount();

        int maxCol = 0;
        for (int i = 0; i < model->rowCount(); i++) {
            maxCol = std::max(maxCol, model->currentLyricLength(i));
        }

        action->m_targetColCount = maxCol;
        return action;
    }

    void ModelShrinkAction::execute() {
        m_model->setColumnCount(m_targetColCount);
    }

    void ModelShrinkAction::undo() {
        m_model->setColumnCount(m_rawColCount);
    }
} // FillLyric