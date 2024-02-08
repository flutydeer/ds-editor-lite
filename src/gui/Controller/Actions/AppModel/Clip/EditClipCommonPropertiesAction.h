//
// Created by fluty on 2024/2/8.
//

#ifndef EDITCLIPCOMMONPROPERTIESACTION_H
#define EDITCLIPCOMMONPROPERTIESACTION_H

#include "Controller/History/IAction.h"
#include "Model/DsClip.h"
#include "Model/DsTrack.h"

class EditClipCommonPropertiesAction : public IAction {
public:
    static EditClipCommonPropertiesAction *build(const DsClip::ClipCommonProperties &oldArgs,
                                                 const DsClip::ClipCommonProperties &newArgs,
                                                 DsClip *clip, DsTrack *track);
    void execute() override;
    void undo() override;

private:
    DsClip::ClipCommonProperties m_oldArgs;
    DsClip::ClipCommonProperties m_newArgs;
    DsClip *m_clip = nullptr;
    DsTrack *m_track = nullptr;
};



#endif // EDITCLIPCOMMONPROPERTIESACTION_H
