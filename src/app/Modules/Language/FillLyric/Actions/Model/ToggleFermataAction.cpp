#include "ToggleFermataAction.h"

namespace FillLyric {

    ToggleFermataAction *ToggleFermataAction::build(PhonicModel *model) {
        const auto action = new ToggleFermataAction;
        action->m_model = model;
        return action;
    }

    void ToggleFermataAction::execute() {
        if (m_model->fermataState) {
            m_model->expandFermata();
        } else {
            m_model->collapseFermata();
        }
        m_model->fermataState = !m_model->fermataState;
        m_model->shrinkModel();
    }

    void ToggleFermataAction::undo() {
        if (m_model->fermataState) {
            m_model->expandFermata();
        } else {
            m_model->collapseFermata();
        }
        m_model->fermataState = !m_model->fermataState;
        m_model->shrinkModel();
    }
} // FillLyric