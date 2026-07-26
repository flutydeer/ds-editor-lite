#include "ImportProjectActions.h"

#include "Controller/Actions/AppModel/Tempo/TempoActions.h"
#include "Controller/Actions/AppModel/TimeSignature/TimeSignatureActions.h"
#include "Controller/Actions/AppModel/Track/InsertTrackAction.h"
#include <lite/ProjectModel/AppModel/AppModel.h>

#include <QCoreApplication>

ImportProjectActions::ImportProjectActions(ProjectModelData &&data, const bool importTempo,
                                           const bool importTimeSignature, AppModel *model) {
    setTranslatableName("ImportProjectActions",
                        QT_TRANSLATE_NOOP("ImportProjectActions", "Import MIDI"));

    const auto oldTempo = model->timeline().tempoAt(0);
    const auto newTempo = data.timeline.tempoAt(0);
    if (importTempo && qAbs(oldTempo - newTempo) > 0.001) {
        const auto actions = new TempoActions;
        actions->editTempo(oldTempo, newTempo, model);
        addAction(actions);
    }
    const auto oldSignature = model->timeline().timeSignatureAt(0);
    const auto newSignature = data.timeline.timeSignatureAt(0);
    if (importTimeSignature && oldSignature != newSignature) {
        const auto actions = new TimeSignatureActions;
        actions->editTimeSignature(oldSignature, newSignature, model);
        addAction(actions);
    }

    auto index = model->tracks().count();
    for (auto &track : data.tracks)
        addAction(InsertTrackAction::build(track.release(), index++, model));
}
