//
// Created by fluty on 2024/2/8.
//

#include "EditClipCommonPropertiesAction.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

namespace {
    void applyArgs(Clip *clip, const Clip::ClipCommonProperties &args) {
        clip->setName(args.name);
        clip->setStart(args.start);
        clip->setClipStart(args.clipStart);
        clip->setLength(args.length);
        clip->setClipLen(args.clipLen);
        if (clip->clipType() == IClip::Audio) {
            // The tick snapshot may predate a tempo change; the ms truth
            // carried in the args re-derives the caches under the current map
            static_cast<AudioClip *>(clip)->applyRealTimeAnchorFromProperties(
                args, appModel->timeline());
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
        const auto &timeline = appModel->timeline();
        if (a->m_oldArgs.playLengthMs < 0)
            AudioClip::deriveTruthForProperties(a->m_oldArgs, timeline);
        if (a->m_newArgs.playLengthMs < 0)
            AudioClip::deriveTruthForProperties(a->m_newArgs, timeline);
    }
    return a;
}

void EditClipCommonPropertiesAction::execute() {
    m_track->removeClip(m_clip);
    applyArgs(m_clip, m_newArgs);
    m_track->insertClip(m_clip);
    m_clip->notifyPropertyChanged();
}

void EditClipCommonPropertiesAction::undo() {
    m_track->removeClip(m_clip);
    applyArgs(m_clip, m_oldArgs);
    m_track->insertClip(m_clip);
    m_clip->notifyPropertyChanged();
}