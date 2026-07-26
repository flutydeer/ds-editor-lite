#include "EditTemposAction.h"

#include <lite/ProjectModel/AppModel/AppModel.h>

EditTemposAction *EditTemposAction::build(const QList<Tempo> &oldTempos,
                                          const QList<Tempo> &newTempos, AppModel *model) {
    const auto a = new EditTemposAction;
    a->m_oldTempos = oldTempos;
    a->m_newTempos = newTempos;
    a->m_model = model;
    return a;
}

void EditTemposAction::execute() {
    auto timeline = m_model->timeline();
    timeline.setTempos(m_newTempos);
    m_model->setTimeline(std::move(timeline));
}

void EditTemposAction::undo() {
    auto timeline = m_model->timeline();
    timeline.setTempos(m_oldTempos);
    m_model->setTimeline(std::move(timeline));
}
