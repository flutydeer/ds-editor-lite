//
// Created by fluty on 2024/2/7.
//

#include "TempoActions.h"

#include "EditTemposAction.h"

#include <lite/ProjectModel/AppModel/AppModel.h>

#include <algorithm>

void TempoActions::setTempoAt(const Tempo &tempo, AppModel *model) {
    const auto &oldTempos = model->timeline().tempos();
    const bool exists =
        std::any_of(oldTempos.cbegin(), oldTempos.cend(),
                    [&](const Tempo &existing) { return existing.pos == tempo.pos; });
    QList<Tempo> newTempos;
    for (const auto &existing : oldTempos) {
        if (existing.pos != tempo.pos)
            newTempos.append(existing);
    }
    newTempos.append(tempo);
    std::sort(newTempos.begin(), newTempos.end(),
              [](const Tempo &a, const Tempo &b) { return a.pos < b.pos; });

    setTranslatableName("TempoActions", exists ? QT_TRANSLATE_NOOP("TempoActions", "Edit Tempo")
                                               : QT_TRANSLATE_NOOP("TempoActions", "Insert Tempo"));
    addAction(EditTemposAction::build(oldTempos, newTempos, model));
}

void TempoActions::removeTempoAt(const int tick, AppModel *model) {
    const auto &oldTempos = model->timeline().tempos();
    QList<Tempo> newTempos;
    for (const auto &existing : oldTempos) {
        if (existing.pos != tick)
            newTempos.append(existing);
    }

    setTranslatableName("TempoActions", QT_TRANSLATE_NOOP("TempoActions", "Remove Tempo"));
    addAction(EditTemposAction::build(oldTempos, newTempos, model));
}
