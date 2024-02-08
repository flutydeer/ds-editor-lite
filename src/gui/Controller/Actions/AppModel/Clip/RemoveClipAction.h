//
// Created by fluty on 2024/2/8.
//

#ifndef REMOVECLIPACTION_H
#define REMOVECLIPACTION_H

#include "Controller/History/IAction.h"
#include "Model/DsClip.h"
#include "Model/DsTrack.h"

class RemoveClipAction final: public IAction {
public:
    static RemoveClipAction *build(DsClip *clip, DsTrack *track);
    void execute() override;
    void undo() override;

private:
    DsClip *m_clip = nullptr;
    DsTrack *m_track = nullptr;
};



#endif // REMOVECLIPACTION_H
