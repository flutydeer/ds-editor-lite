//
// Created by fluty on 2024/2/8.
//

#include <QDebug>

#include "ClipActions.h"
#include "EditClipCommonPropertiesAction.h"
#include "EditSingingClipPropertiesAction.h"
#include "InsertClipAction.h"
#include "RemoveClipAction.h"

void ClipActions::insertClips(const QList<Clip *> &clips, Track *track) {
    for (const auto clip : clips)
        addAction(InsertClipAction::build(clip, track));
}
void ClipActions::removeClips(const QList<Clip *> &clips, Track *track) {
    for (const auto clip : clips)
        addAction(RemoveClipAction::build(clip, track));
}
void ClipActions::editSingingClipProperties(const QList<Clip::ClipCommonProperties> &oldArgs,
                                            const QList<Clip::ClipCommonProperties> &newArgs,
                                            const QList<DsSingingClip *> &clips, Track *track) {
    // TODO: edit singer name and move params
    int i = 0;
    for (const auto clip : clips) {
        addAction(EditSingingClipPropertiesAction::build(oldArgs[i], newArgs[i], clip, track));
        i++;
    }
}
void ClipActions::editAudioClipProperties(const QList<Clip::ClipCommonProperties> &oldArgs,
                                          const QList<Clip::ClipCommonProperties> &newArgs,
                                          const QList<DsAudioClip *> &clips, Track *track) {
    int i = 0;
    for (const auto clip : clips) {
        addAction(EditClipCommonPropertiesAction::build(oldArgs[i], newArgs[i], clip, track));
        i++;
    }
}