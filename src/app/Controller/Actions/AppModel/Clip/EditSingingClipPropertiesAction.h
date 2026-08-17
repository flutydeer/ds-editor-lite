#ifndef EDITSINGINGCLIPPROPERTIES_H
#define EDITSINGINGCLIPPROPERTIES_H

#include <lite/ProjectModel/AppModel/Clip.h>
#include <lite/History/IAction.h>
#include "Model/AppModel/SingingClipPhonemeNormalizer.h"


class SingingClip;
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
    QList<SingingClipPhonemeNormalizer::ResetRecord> m_resetRecords;
    SingingClip *m_clip = nullptr;
    Track *m_track = nullptr;
};



#endif // EDITSINGINGCLIPPROPERTIES_H
