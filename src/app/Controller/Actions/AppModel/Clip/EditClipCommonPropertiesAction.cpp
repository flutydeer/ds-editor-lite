#include "EditClipCommonPropertiesAction.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <algorithm>

namespace {
    void applyArgs(Clip *clip, const Clip::ClipCommonProperties &args,
                   const bool updateAudioTiming) {
        // opendspx requires clip.pos = start + clipStart to stay non-negative.
        auto safeArgs = Clip::ClipCommonProperties(args);
        safeArgs.start = std::max(safeArgs.start, -safeArgs.clipStart);
        clip->setName(safeArgs.name);
        clip->setStart(safeArgs.start);
        clip->setClipStart(safeArgs.clipStart);
        clip->setLength(safeArgs.length);
        clip->setClipLen(safeArgs.clipLen);
        clip->setGain(safeArgs.gain);
        clip->setMute(safeArgs.mute);
        if (clip->clipType() == IClip::Audio && updateAudioTiming) {
            // The tick snapshot may predate a tempo change; the ms truth
            // carried in the args re-derives the caches under the current map
            static_cast<AudioClip *>(clip)->applyRealTimeAnchorFromProperties(safeArgs,
                                                                              appModel->timeline());
        }
    }
}

EditClipCommonPropertiesAction *
    EditClipCommonPropertiesAction::build(const Clip::ClipCommonProperties &oldArgs,
                                          const Clip::ClipCommonProperties &newArgs, Clip *clip,
                                          Track *track) {
    auto a = new EditClipCommonPropertiesAction;
    a->m_oldArgs = oldArgs;
    a->m_newArgs = newArgs;
    a->m_clip = clip;
    a->m_track = track;
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
        a->m_audioTimingChanged = !AudioClip::timingPropertiesEqual(a->m_oldArgs, a->m_newArgs);
    }
    return a;
}

void EditClipCommonPropertiesAction::execute() {
    m_track->removeClip(m_clip);
    applyArgs(m_clip, m_newArgs, m_audioTimingChanged);
    m_track->insertClip(m_clip);
    m_clip->notifyPropertyChanged();
}

void EditClipCommonPropertiesAction::undo() {
    m_track->removeClip(m_clip);
    applyArgs(m_clip, m_oldArgs, m_audioTimingChanged);
    m_track->insertClip(m_clip);
    m_clip->notifyPropertyChanged();
}
