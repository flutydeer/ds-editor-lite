#include "MoveClipToTrackAction.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

namespace {
    void applyArgs(Clip *clip, const Clip::ClipCommonProperties &args) {
        clip->setName(args.name);
        clip->setStart(args.start);
        clip->setClipStart(args.clipStart);
        clip->setLength(args.length);
        clip->setClipLen(args.clipLen);
        clip->setGain(args.gain);
        clip->setMute(args.mute);
        if (clip->clipType() == IClip::Audio) {
            static_cast<AudioClip *>(clip)->applyRealTimeAnchorFromProperties(args,
                                                                              appModel->timeline());
        }
    }
}

MoveClipToTrackAction *MoveClipToTrackAction::build(const Clip::ClipCommonProperties &oldArgs,
                                                    const Clip::ClipCommonProperties &newArgs,
                                                    Clip *clip, Track *oldTrack, Track *newTrack) {
    const auto a = new MoveClipToTrackAction;
    a->m_oldArgs = oldArgs;
    a->m_newArgs = newArgs;
    a->m_clip = clip;
    a->m_oldTrack = oldTrack;
    a->m_newTrack = newTrack;
    if (clip->clipType() == IClip::Audio) {
        const auto audioClip = static_cast<AudioClip *>(clip);
        const auto &timeline = appModel->timeline();
        if (a->m_oldArgs.playLengthMs < 0) {
            if (audioClip->hasRealTimeAnchor()) {
                a->m_oldArgs.trimStartMs = audioClip->trimStartMs();
                a->m_oldArgs.playLengthMs = audioClip->playLengthMs();
                a->m_oldArgs.materialLengthMs = audioClip->materialLengthMs();
            } else {
                AudioClip::deriveTruthForProperties(a->m_oldArgs, timeline);
            }
        }
        if (a->m_newArgs.playLengthMs < 0) {
            AudioClip::deriveTruthForProperties(a->m_newArgs, timeline);
            AudioClip::preserveUnchangedTruth(a->m_newArgs, a->m_oldArgs);
        }
    }
    return a;
}

void MoveClipToTrackAction::execute() {
    SingingClipPhonemeNormalizer::WordStates previousWordStates;
    if (m_clip->clipType() == IClip::Singing)
        previousWordStates =
            SingingClipPhonemeNormalizer::captureWordStates(*static_cast<SingingClip *>(m_clip));
    m_oldTrack->removeClip(m_clip);
    applyArgs(m_clip, m_newArgs);
    m_newTrack->insertClip(m_clip);
    if (m_clip->clipType() == IClip::Singing) {
        const auto singingClip = static_cast<SingingClip *>(m_clip);
        singingClip->setTrackVoiceContext(m_newTrack->singerInfo(), m_newTrack->speakerInfo(),
                                          m_newTrack->speakerMixData());
        m_resetRecords = previousWordStates.isEmpty()
                             ? QList<SingingClipPhonemeNormalizer::ResetRecord>{}
                             : SingingClipPhonemeNormalizer::normalizeEditedOffsets(
                                   *singingClip, previousWordStates);
    } else {
        m_resetRecords.clear();
    }
    m_clip->notifyPropertyChanged();
    if (!m_resetRecords.isEmpty()) {
        const auto singingClip = static_cast<SingingClip *>(m_clip);
        singingClip->notifyNoteChanged(
            SingingClip::EditedPhonemeOffsetChange,
            SingingClipPhonemeNormalizer::notesFromResetRecords(m_resetRecords));
    }
    m_oldTrack->notifyClipChanged(Track::Removed, m_clip);
    m_newTrack->notifyClipChanged(Track::Inserted, m_clip);
}

void MoveClipToTrackAction::undo() {
    m_newTrack->removeClip(m_clip);
    applyArgs(m_clip, m_oldArgs);
    m_oldTrack->insertClip(m_clip);
    if (m_clip->clipType() == IClip::Singing) {
        const auto singingClip = static_cast<SingingClip *>(m_clip);
        singingClip->setTrackVoiceContext(m_oldTrack->singerInfo(), m_oldTrack->speakerInfo(),
                                          m_oldTrack->speakerMixData());
        SingingClipPhonemeNormalizer::restoreEditedOffsets(m_resetRecords);
    }
    m_clip->notifyPropertyChanged();
    if (!m_resetRecords.isEmpty()) {
        const auto singingClip = static_cast<SingingClip *>(m_clip);
        singingClip->notifyNoteChanged(
            SingingClip::EditedPhonemeOffsetChange,
            SingingClipPhonemeNormalizer::notesFromResetRecords(m_resetRecords));
    }
    m_newTrack->notifyClipChanged(Track::Removed, m_clip);
    m_oldTrack->notifyClipChanged(Track::Inserted, m_clip);
}
