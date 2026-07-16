#include "ImportProjectActions.h"

#include "Controller/Actions/AppModel/Tempo/TempoActions.h"
#include "Controller/Actions/AppModel/TimeSignature/TimeSignatureActions.h"
#include "Controller/Actions/AppModel/Track/InsertTrackAction.h"
#include "Model/AppModel/AppModel.h"

#include <QCoreApplication>

ImportProjectActions::ImportProjectActions(ProjectModelData &&data, const bool importTempo,
                                           const bool importTimeSignature, AppModel *model) {
    setTranslatableName("ImportProjectActions",
                        QT_TRANSLATE_NOOP("ImportProjectActions", "Import MIDI"));

    if (importTempo && qAbs(model->tempo() - data.tempo) > 0.001) {
        const auto actions = new TempoActions;
        actions->editTempo(model->tempo(), data.tempo, model);
        addAction(actions);
    }
    if (importTimeSignature && model->timeSignature() != data.timeSignature) {
        const auto actions = new TimeSignatureActions;
        actions->editTimeSignature(model->timeSignature(), data.timeSignature, model);
        addAction(actions);
    }

    auto index = model->tracks().count();
    for (auto &track : data.tracks)
        addAction(InsertTrackAction::build(track.release(), index++, model));
}
