//
// Created by fluty on 2024/2/8.
//

#ifndef MOVECLIPTOTRACKACTION_H
#define MOVECLIPTOTRACKACTION_H

#include "Model/AppModel/Clip.h"
#include "Modules/History/IAction.h"

class Track;

class MoveClipToTrackAction : public IAction {
public:
    static MoveClipToTrackAction *build(const Clip::ClipCommonProperties &oldArgs,
                                        const Clip::ClipCommonProperties &newArgs,
                                        Clip *clip, Track *oldTrack, Track *newTrack);
    void execute() override;
    void undo() override;

private:
    Clip::ClipCommonProperties m_oldArgs;
    Clip::ClipCommonProperties m_newArgs;
    Clip *m_clip = nullptr;
    Track *m_oldTrack = nullptr;
    Track *m_newTrack = nullptr;
};

#endif // MOVECLIPTOTRACKACTION_H
