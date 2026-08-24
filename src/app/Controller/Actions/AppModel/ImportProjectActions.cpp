#include "ImportProjectActions.h"

#include "Controller/Actions/AppModel/Tempo/EditTemposAction.h"
#include "Controller/Actions/AppModel/TimeSignature/EditTimeSignaturesAction.h"
#include "Controller/Actions/AppModel/Track/InsertTrackAction.h"
#include <lite/ProjectModel/AppModel/AppModel.h>

#include <QCoreApplication>

ImportProjectActions::ImportProjectActions(ProjectModelData &&data, const bool importTempo,
                                           const bool importTimeSignature, AppModel *model) {
    setTranslatableName("ImportProjectActions",
                        QT_TRANSLATE_NOOP("ImportProjectActions", "Import MIDI"));

    const auto &currentTimeline = model->timeline();
    if (importTempo && currentTimeline.tempos() != data.timeline.tempos()) {
        // Audio clips are re-anchored inside AppModel::setTimeline; a full
        // sequence replacement covers the single-point case as well
        addAction(EditTemposAction::build(currentTimeline.tempos(), data.timeline.tempos(), model));
    }
    if (importTimeSignature && currentTimeline.timeSignatures() != data.timeline.timeSignatures()) {
        addAction(EditTimeSignaturesAction::build(currentTimeline.timeSignatures(),
                                                  data.timeline.timeSignatures(), model));
    }

    auto index = model->tracks().count();
    for (auto &track : data.tracks)
        addAction(InsertTrackAction::build(track.release(), index++, model));
}
