//
// Created by fluty on 2024/2/8.
//

#ifndef EDITSINGINGCLIPPROPERTIES_H
#define EDITSINGINGCLIPPROPERTIES_H

#include "Controller/History/IAction.h"
#include "Model/DsClip.h"
#include "Model/DsTrack.h"

class EditSingingClipPropertiesAction : public IAction {
public:
    static EditSingingClipPropertiesAction *build(const DsClip::ClipCommonProperties &oldArgs,
                                            const DsClip::ClipCommonProperties &newArgs,
                                            DsSingingClip *clip, DsTrack *track);
    void execute() override;
    void undo() override;

private:
    DsClip::ClipCommonProperties m_oldArgs;
    DsClip::ClipCommonProperties m_newArgs;
    DsSingingClip *m_clip = nullptr;
    DsTrack *m_track = nullptr;
};



#endif // EDITSINGINGCLIPPROPERTIES_H
