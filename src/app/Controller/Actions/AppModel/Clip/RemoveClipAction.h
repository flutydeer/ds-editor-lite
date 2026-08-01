#ifndef REMOVECLIPACTION_H
#define REMOVECLIPACTION_H

#include <lite/History/IAction.h>

class Track;
class Clip;

class RemoveClipAction final : public IAction {
public:
    static RemoveClipAction *build(Clip *clip, Track *track);
    void execute() override;
    void undo() override;

private:
    Clip *m_clip = nullptr;
    Track *m_track = nullptr;
};



#endif // REMOVECLIPACTION_H
