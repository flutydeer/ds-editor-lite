#include "ClipActions.h"

#include "EditAudioClipPathAction.h"
#include "EditClipCommonPropertiesAction.h"
#include "EditSingingClipPropertiesAction.h"
#include "InsertClipAction.h"
#include "MoveClipToTrackAction.h"
#include "RemoveClipAction.h"
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

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

void ClipActions::editClips(const QList<ClipPropertyEdit> &edits) {
    if (edits.isEmpty())
        return;

    // The name describes what the whole batch did, so a gesture that mixes clip types still shows
    // one honest entry in the undo menu. The focus transition is merged for the same reason: undo
    // has to reveal every clip the batch touched, not just the last one.
    bool allSinging = true;
    bool allAudio = true;
    bool allCrossTrack = true;
    for (const auto &edit : edits) {
        const auto type = edit.clip->clipType();
        allSinging = allSinging && type == IClip::Singing;
        allAudio = allAudio && type == IClip::Audio;
        allCrossTrack = allCrossTrack && edit.newTrack != edit.oldTrack;
    }
    if (allSinging)
        setTranslatableName("ClipActions",
                            QT_TRANSLATE_NOOP("ClipActions", "Edit singing clip(s)"));
    else if (allAudio)
        setTranslatableName("ClipActions", QT_TRANSLATE_NOOP("ClipActions", "Edit audio clip(s)"));
    else if (allCrossTrack)
        setTranslatableName("ClipActions", QT_TRANSLATE_NOOP("ClipActions", "Move clip to track"));
    else
        setTranslatableName("ClipActions", QT_TRANSLATE_NOOP("ClipActions", "Edit clip(s)"));

    QList<HistoryFocus> before;
    QList<HistoryFocus> after;
    before.reserve(edits.size());
    after.reserve(edits.size());
    for (const auto &edit : edits) {
        if (edit.newTrack != edit.oldTrack) {
            addAction(MoveClipToTrackAction::build(edit.oldArgs, edit.newArgs, edit.clip,
                                                   edit.oldTrack, edit.newTrack));
        } else if (edit.clip->clipType() == Clip::Audio) {
            addAction(EditClipCommonPropertiesAction::build(
                edit.oldArgs, edit.newArgs, static_cast<AudioClip *>(edit.clip), edit.oldTrack));
        } else {
            addAction(EditSingingClipPropertiesAction::build(
                edit.oldArgs, edit.newArgs, static_cast<SingingClip *>(edit.clip), edit.oldTrack));
        }
        before.append(clipFocus(edit.oldArgs, edit.oldTrack));
        after.append(clipFocus(edit.newArgs, edit.newTrack));
    }
    setFocusTransition({mergeFocus(before), mergeFocus(after)});
}

void ClipActions::editSingingClipProperties(const QList<Clip::ClipCommonProperties> &oldArgs,
                                            const QList<Clip::ClipCommonProperties> &newArgs,
                                            const QList<SingingClip *> &clips,
                                            const QList<Track *> &tracks) {
    QList<ClipPropertyEdit> edits;
    edits.reserve(clips.size());
    for (qsizetype i = 0; i < clips.size(); ++i)
        edits.append({oldArgs[i], newArgs[i], clips[i], tracks[i], tracks[i]});
    editClips(edits);
}

void ClipActions::editAudioClipProperties(const QList<Clip::ClipCommonProperties> &oldArgs,
                                          const QList<Clip::ClipCommonProperties> &newArgs,
                                          const QList<AudioClip *> &clips,
                                          const QList<Track *> &tracks) {
    QList<ClipPropertyEdit> edits;
    edits.reserve(clips.size());
    for (qsizetype i = 0; i < clips.size(); ++i)
        edits.append({oldArgs[i], newArgs[i], clips[i], tracks[i], tracks[i]});
    editClips(edits);
}

void ClipActions::splitClip(const Clip::ClipCommonProperties &oldArgs,
                            const Clip::ClipCommonProperties &newArgs, Clip *clip, Clip *newClip,
                            Track *track) {
    setTranslatableName("ClipActions", QT_TRANSLATE_NOOP("ClipActions", "Split clip"));
    if (clip->clipType() == Clip::Audio) {
        addAction(EditClipCommonPropertiesAction::build(oldArgs, newArgs,
                                                        static_cast<AudioClip *>(clip), track));
    } else {
        addAction(EditSingingClipPropertiesAction::build(oldArgs, newArgs,
                                                         static_cast<SingingClip *>(clip), track));
    }
    addAction(InsertClipAction::build(newClip, track));
    const auto before = clipFocus(oldArgs, track);
    const auto after = mergeFocus(QList<HistoryFocus>{
        clipFocus(newArgs, track), clipFocus(Clip::ClipCommonProperties(*newClip), track)});
    setFocusTransition({before, after});
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
    editClips({
        {oldArgs, newArgs, clip, oldTrack, newTrack}
    });
}
