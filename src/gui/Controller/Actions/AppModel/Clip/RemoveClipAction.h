//
// Created by fluty on 2024/2/8.
//

#ifndef REMOVECLIPACTION_H
#define REMOVECLIPACTION_H

#include "Controller/History/IAction.h"

class Track;
class Clip;

class RemoveClipAction final: public IAction {
public:
    static RemoveClipAction *build(Clip *clip, Track *track);
    void execute() override;
    void undo() override;

private:
    Clip *m_clip = nullptr;
    Track *m_track = nullptr;
};



#endif // REMOVECLIPACTION_H
