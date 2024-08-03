//
// Created by fluty on 2024/2/8.
//

#ifndef EDITSINGINGCLIPPROPERTIES_H
#define EDITSINGINGCLIPPROPERTIES_H

#include "Model/AppModel/Clip.h"
#include "Modules/History/IAction.h"

class Track;

class EditSingingClipPropertiesAction : public IAction {
public:
    static EditSingingClipPropertiesAction *build(const Clip::ClipCommonProperties &oldArgs,
                                                  const Clip::ClipCommonProperties &newArgs,
                                                  SingingClip *clip, Track *track);
    void execute() override;
    void undo() override;

private:
    Clip::ClipCommonProperties m_oldArgs;
    Clip::ClipCommonProperties m_newArgs;
    SingingClip *m_clip = nullptr;
    Track *m_track = nullptr;
};



#endif // EDITSINGINGCLIPPROPERTIES_H
