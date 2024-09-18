//
// Created by fluty on 2024/2/8.
//

#include "ClipActions.h"

#include "EditClipCommonPropertiesAction.h"
#include "EditSingingClipPropertiesAction.h"
#include "InsertClipAction.h"
#include "RemoveClipAction.h"
#include "Model/AppModel/AudioClip.h"

void ClipActions::insertClips(const QList<Clip *> &clips, Track *track) {
    setName(tr("Insert clip(s)"));
    for (const auto clip : clips)
        addAction(InsertClipAction::build(clip, track));
}

void ClipActions::removeClips(const QList<Clip *> &clips, const QList<Track *> &tracks) {
    setName(tr("Remove clip(s)"));
    int i = 0;
    for (const auto clip : clips) {
        addAction(RemoveClipAction::build(clip, tracks[i]));
        i++;
    }
}

void ClipActions::editSingingClipProperties(const QList<Clip::ClipCommonProperties> &oldArgs,
                                            const QList<Clip::ClipCommonProperties> &newArgs,
                                            const QList<SingingClip *> &clips,
                                            const QList<Track *> &tracks) {
    setName(tr("Edit singing clip(s)"));
    // TODO: edit singer name and move params
    int i = 0;
    for (const auto clip : clips) {
        addAction(EditSingingClipPropertiesAction::build(oldArgs[i], newArgs[i], clip, tracks[i]));
        i++;
    }
}

void ClipActions::editAudioClipProperties(const QList<Clip::ClipCommonProperties> &oldArgs,
                                          const QList<Clip::ClipCommonProperties> &newArgs,
                                          const QList<AudioClip *> &clips,
                                          const QList<Track *> &tracks) {
    setName(tr("Edit audio clip(s)"));
    int i = 0;
    for (const auto clip : clips) {
        addAction(EditClipCommonPropertiesAction::build(oldArgs[i], newArgs[i], clip, tracks[i]));
        i++;
    }
}