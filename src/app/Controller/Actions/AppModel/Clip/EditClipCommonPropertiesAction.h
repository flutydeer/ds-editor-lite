//
// Created by fluty on 2024/2/8.
//

#ifndef EDITCLIPCOMMONPROPERTIESACTION_H
#define EDITCLIPCOMMONPROPERTIESACTION_H

#include "Model/AppModel/Clip.h"
#include "Modules/History/IAction.h"

class Track;

class EditClipCommonPropertiesAction : public IAction {
public:
    static EditClipCommonPropertiesAction *build(const Clip::ClipCommonProperties &oldArgs,
                                                 const Clip::ClipCommonProperties &newArgs,
                                                 Clip *clip, Track *track);
    void execute() override;
    void undo() override;

private:
    Clip::ClipCommonProperties m_oldArgs;
    Clip::ClipCommonProperties m_newArgs;
    Clip *m_clip = nullptr;
    Track *m_track = nullptr;
};



#endif // EDITCLIPCOMMONPROPERTIESACTION_H
