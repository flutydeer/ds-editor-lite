//
// Created by fluty on 2024/2/8.
//

#ifndef INSERTCLIPACTION_H
#define INSERTCLIPACTION_H

#include "Controller/History/IAction.h"
#include "Model/DsClip.h"
#include "Model/DsTrack.h"

class InsertClipAction final : public IAction {
public:
    static InsertClipAction *build(DsClip *clip, DsTrack *track);
    void execute() override;
    void undo() override;

private:
    DsClip *m_clip = nullptr;
    DsTrack *m_track = nullptr;
};



#endif // INSERTCLIPACTION_H
