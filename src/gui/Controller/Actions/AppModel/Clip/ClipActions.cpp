//
// Created by fluty on 2024/2/8.
//

#include <QDebug>

#include "ClipActions.h"
#include "EditClipCommonPropertiesAction.h"
#include "EditSingingClipPropertiesAction.h"
#include "InsertClipAction.h"
#include "RemoveClipAction.h"

void ClipActions::insertClips(const QList<DsClip *> &clips, DsTrack *track) {
    for (const auto clip : clips)
        addAction(InsertClipAction::build(clip, track));
}
void ClipActions::removeClips(const QList<DsClip *> &clips, DsTrack *track) {
    for (const auto clip : clips)
        addAction(RemoveClipAction::build(clip, track));
}
void ClipActions::editSingingClipProperties(const QList<DsClip::ClipCommonProperties> &oldArgs,
                                            const QList<DsClip::ClipCommonProperties> &newArgs,
                                            const QList<DsSingingClip *> &clips, DsTrack *track) {
    // TODO: edit singer name and move params
    int i = 0;
    for (const auto clip : clips) {
        addAction(EditSingingClipPropertiesAction::build(oldArgs[i], newArgs[i], clip, track));
        i++;
    }
}
void ClipActions::editAudioClipProperties(const QList<DsClip::ClipCommonProperties> &oldArgs,
                                          const QList<DsClip::ClipCommonProperties> &newArgs,
                                          const QList<DsAudioClip *> &clips, DsTrack *track) {
    int i = 0;
    for (const auto clip : clips) {
        addAction(EditClipCommonPropertiesAction::build(oldArgs[i], newArgs[i], clip, track));
        i++;
    }
}