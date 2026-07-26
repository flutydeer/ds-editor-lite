#include "ImportProjectActions.h"

#include "Controller/Actions/AppModel/Tempo/EditTemposAction.h"
#include "Controller/Actions/AppModel/Tempo/TempoActions.h"
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
        const bool bothSinglePoint =
            currentTimeline.tempos().size() == 1 && data.timeline.tempos().size() == 1;
        if (bothSinglePoint) {
            // The single-point path keeps the legacy behavior of re-anchoring
            // audio clips to the new tempo.
            const auto oldTempo = currentTimeline.tempoAt(0);
            const auto newTempo = data.timeline.tempoAt(0);
            if (qAbs(oldTempo - newTempo) > 0.001) {
                const auto actions = new TempoActions;
                actions->editTempo(oldTempo, newTempo, model);
                addAction(actions);
            }
        } else {
            // Multi-point sources replace the whole tempo sequence. Audio clip
            // re-anchoring under tempo maps arrives with the audio engine work.
            addAction(
                EditTemposAction::build(currentTimeline.tempos(), data.timeline.tempos(), model));
        }
    }
    if (importTimeSignature &&
        currentTimeline.timeSignatures() != data.timeline.timeSignatures()) {
        addAction(EditTimeSignaturesAction::build(currentTimeline.timeSignatures(),
                                                  data.timeline.timeSignatures(), model));
    }

    auto index = model->tracks().count();
    for (auto &track : data.tracks)
        addAction(InsertTrackAction::build(track.release(), index++, model));
}
