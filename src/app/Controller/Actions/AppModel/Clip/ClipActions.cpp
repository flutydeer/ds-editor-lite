//
// Created by fluty on 2024/2/8.
//

#include "ClipActions.h"

#include "EditClipCommonPropertiesAction.h"
#include "EditSingingClipPropertiesAction.h"
#include "InsertClipAction.h"
#include "MoveClipToTrackAction.h"
#include "RemoveClipAction.h"
#include "Model/AppModel/AudioClip.h"

#include <QCoreApplication>

void ClipActions::insertClips(const QList<Clip *> &clips, Track *track) {
    setTranslatableName("ClipActions", QT_TRANSLATE_NOOP("ClipActions", "Insert clip(s)"));
    for (const auto clip : clips)
        addAction(InsertClipAction::build(clip, track));
}

void ClipActions::insertClips(const QList<Clip *> &clips, const QList<Track *> &tracks) {
    setTranslatableName("ClipActions", QT_TRANSLATE_NOOP("ClipActions", "Insert clip(s)"));
    for (int i = 0; i < clips.count(); i++)
        addAction(InsertClipAction::build(clips[i], tracks[i]));
}

void ClipActions::removeClips(const QList<Clip *> &clips, const QList<Track *> &tracks) {
    setTranslatableName("ClipActions", QT_TRANSLATE_NOOP("ClipActions", "Remove clip(s)"));
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
    setTranslatableName("ClipActions", QT_TRANSLATE_NOOP("ClipActions", "Edit singing clip(s)"));
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
    setTranslatableName("ClipActions", QT_TRANSLATE_NOOP("ClipActions", "Edit audio clip(s)"));
    int i = 0;
    for (const auto clip : clips) {
        addAction(EditClipCommonPropertiesAction::build(oldArgs[i], newArgs[i], clip, tracks[i]));
        i++;
    }
}

void ClipActions::moveClipToTrack(const Clip::ClipCommonProperties &oldArgs,
                                  const Clip::ClipCommonProperties &newArgs, Clip *clip,
                                  Track *oldTrack, Track *newTrack) {
    setTranslatableName("ClipActions", QT_TRANSLATE_NOOP("ClipActions", "Move clip to track"));
    addAction(MoveClipToTrackAction::build(oldArgs, newArgs, clip, oldTrack, newTrack));
}
