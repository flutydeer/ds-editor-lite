#include "BatchImportActions.h"

#include "Controller/Actions/AppModel/Clip/InsertClipAction.h"
#include "Controller/Actions/AppModel/Tempo/EditTemposAction.h"
#include "Controller/Actions/AppModel/TimeSignature/EditTimeSignaturesAction.h"
#include "Controller/Actions/AppModel/Track/InsertTrackAction.h"

#include <lite/ProjectModel/AppModel/AppModel.h>

BatchImportActions *BatchImportActions::build(const QList<Tempo> &oldTempos,
                                              const QList<Tempo> &newTempos,
                                              const QList<TimeSignature> &oldSignatures,
                                              const QList<TimeSignature> &newSignatures,
                                              const QList<Item> &items, AppModel *model) {
    const auto a = new BatchImportActions;
    a->setTranslatableName("BatchImportActions",
                           QT_TRANSLATE_NOOP("BatchImportActions", "Import files"));

    if (oldTempos != newTempos)
        a->addAction(EditTemposAction::build(oldTempos, newTempos, model));
    if (oldSignatures != newSignatures)
        a->addAction(EditTimeSignaturesAction::build(oldSignatures, newSignatures, model));

    int newTrackOrdinal = 0;
    for (const auto &item : items) {
        if (item.newTrack) {
            // New tracks are appended at the end; the ordinal accounts for the
            // tracks this very batch inserts before it.
            a->addAction(InsertTrackAction::build(item.newTrack,
                                                  model->tracks().size() + newTrackOrdinal, model));
            newTrackOrdinal++;
        } else if (item.existingTrack) {
            a->addAction(InsertClipAction::build(item.clip, item.existingTrack));
        }
    }
    return a;
}
