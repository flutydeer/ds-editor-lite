//
// Created by fluty on 2024/2/8.
//

#include "ClipActions.h"

#include "EditAudioClipPathAction.h"
#include "EditClipCommonPropertiesAction.h"
#include "EditSingingClipPropertiesAction.h"
#include "InsertClipAction.h"
#include "MoveClipToTrackAction.h"
#include "RemoveClipAction.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/AudioClip.h"
#include "Model/AppModel/Track.h"

#include <QCoreApplication>

#include <limits>

namespace {

    HistoryFocus clipFocus(const Clip::ClipCommonProperties &args, Track *track) {
        HistoryFocus focus;
        focus.kind = HistoryFocusKind::TrackClips;
        focus.objectIds = {args.id};
        focus.trackId = track ? track->id() : -1;
        focus.trackIndex = track ? appModel->tracks().indexOf(track) : -1;
        focus.ticksAreLocal = false;
        focus.tickStart = args.start + args.clipStart;
        focus.tickEnd = focus.tickStart + args.clipLen;
        focus.valueStart = focus.valueEnd = focus.trackIndex;
        return focus;
    }

    HistoryFocus clipFocus(const Clip *clip, Track *track) {
        return clipFocus(Clip::ClipCommonProperties(*clip), track);
    }

    HistoryFocus mergeFocus(const QList<HistoryFocus> &focuses) {
        if (focuses.isEmpty())
            return {};
        auto result = focuses.first();
        result.objectIds.clear();
        result.tickStart = std::numeric_limits<double>::max();
        result.tickEnd = std::numeric_limits<double>::lowest();
        result.valueStart = std::numeric_limits<double>::max();
        result.valueEnd = std::numeric_limits<double>::lowest();
        for (const auto &focus : focuses) {
            result.objectIds.append(focus.objectIds);
            result.tickStart = qMin(result.tickStart, focus.tickStart);
            result.tickEnd = qMax(result.tickEnd, focus.tickEnd);
            result.valueStart = qMin(result.valueStart, focus.valueStart);
            result.valueEnd = qMax(result.valueEnd, focus.valueEnd);
            if (result.trackId != focus.trackId)
                result.trackId = -1;
        }
        result.trackIndex = qRound((result.valueStart + result.valueEnd) / 2.0);
        return result;
    }

} // namespace

void ClipActions::insertClips(const QList<Clip *> &clips, Track *track) {
    setTranslatableName("ClipActions", QT_TRANSLATE_NOOP("ClipActions", "Insert clip(s)"));
    for (const auto clip : clips)
        addAction(InsertClipAction::build(clip, track));
    QList<HistoryFocus> focuses;
    for (const auto clip : clips)
        focuses.append(clipFocus(clip, track));
    const auto focus = mergeFocus(focuses);
    setFocusTransition({focus, focus});
}

void ClipActions::insertClips(const QList<Clip *> &clips, const QList<Track *> &tracks) {
    setTranslatableName("ClipActions", QT_TRANSLATE_NOOP("ClipActions", "Insert clip(s)"));
    for (int i = 0; i < clips.count(); i++)
        addAction(InsertClipAction::build(clips[i], tracks[i]));
    QList<HistoryFocus> focuses;
    for (int i = 0; i < clips.count(); i++)
        focuses.append(clipFocus(clips[i], tracks[i]));
    const auto focus = mergeFocus(focuses);
    setFocusTransition({focus, focus});
}

void ClipActions::removeClips(const QList<Clip *> &clips, const QList<Track *> &tracks) {
    setTranslatableName("ClipActions", QT_TRANSLATE_NOOP("ClipActions", "Remove clip(s)"));
    int i = 0;
    for (const auto clip : clips) {
        addAction(RemoveClipAction::build(clip, tracks[i]));
        i++;
    }
    QList<HistoryFocus> focuses;
    for (int j = 0; j < clips.count(); j++)
        focuses.append(clipFocus(clips[j], tracks[j]));
    const auto focus = mergeFocus(focuses);
    setFocusTransition({focus, focus});
}

/* Keep the action construction above readable; all property variants share the same focus. */
void ClipActions::editSingingClipProperties(const QList<Clip::ClipCommonProperties> &oldArgs,
                                            const QList<Clip::ClipCommonProperties> &newArgs,
                                            const QList<SingingClip *> &clips,
                                            const QList<Track *> &tracks) {
    setTranslatableName("ClipActions", QT_TRANSLATE_NOOP("ClipActions", "Edit singing clip(s)"));
    int i = 0;
    QList<HistoryFocus> before;
    QList<HistoryFocus> after;
    for (const auto clip : clips) {
        addAction(EditSingingClipPropertiesAction::build(oldArgs[i], newArgs[i], clip, tracks[i]));
        before.append(clipFocus(oldArgs[i], tracks[i]));
        after.append(clipFocus(newArgs[i], tracks[i]));
        i++;
    }
    setFocusTransition({mergeFocus(before), mergeFocus(after)});
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
    QList<HistoryFocus> before;
    QList<HistoryFocus> after;
    for (int j = 0; j < clips.count(); j++) {
        before.append(clipFocus(oldArgs[j], tracks[j]));
        after.append(clipFocus(newArgs[j], tracks[j]));
    }
    setFocusTransition({mergeFocus(before), mergeFocus(after)});
}

void ClipActions::relocateAudioClip(AudioClip *clip, const QString &newPath,
                                    const AudioPathInfo &newPathInfo,
                                    const QJsonObject &newFormatData) {
    setTranslatableName("ClipActions", QT_TRANSLATE_NOOP("ClipActions", "Relocate audio file"));
    addAction(EditAudioClipPathAction::build(clip, newPath, newPathInfo, newFormatData));
    Track *track = nullptr;
    appModel->findClipById(clip->id(), track);
    const auto focus = clipFocus(clip, track);
    setFocusTransition({focus, focus});
}

void ClipActions::moveClipToTrack(const Clip::ClipCommonProperties &oldArgs,
                                  const Clip::ClipCommonProperties &newArgs, Clip *clip,
                                  Track *oldTrack, Track *newTrack) {
    setTranslatableName("ClipActions", QT_TRANSLATE_NOOP("ClipActions", "Move clip to track"));
    addAction(MoveClipToTrackAction::build(oldArgs, newArgs, clip, oldTrack, newTrack));
    setFocusTransition({clipFocus(oldArgs, oldTrack), clipFocus(newArgs, newTrack)});
}
