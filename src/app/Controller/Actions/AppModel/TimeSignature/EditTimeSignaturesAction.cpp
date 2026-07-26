#include "EditTimeSignaturesAction.h"

#include <lite/ProjectModel/AppModel/AppModel.h>

EditTimeSignaturesAction *EditTimeSignaturesAction::build(
    const QList<TimeSignature> &oldSignatures, const QList<TimeSignature> &newSignatures,
    AppModel *model) {
    const auto a = new EditTimeSignaturesAction;
    a->m_oldSignatures = oldSignatures;
    a->m_newSignatures = newSignatures;
    a->m_model = model;
    return a;
}

void EditTimeSignaturesAction::execute() {
    auto timeline = m_model->timeline();
    timeline.setTimeSignatures(m_newSignatures);
    m_model->setTimeline(std::move(timeline));
}

void EditTimeSignaturesAction::undo() {
    auto timeline = m_model->timeline();
    timeline.setTimeSignatures(m_oldSignatures);
    m_model->setTimeline(std::move(timeline));
}
