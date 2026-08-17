#include "EditSingingClipPropertiesAction.h"

#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <algorithm>

EditSingingClipPropertiesAction *
    EditSingingClipPropertiesAction::build(const Clip::ClipCommonProperties &oldArgs,
                                           const Clip::ClipCommonProperties &newArgs,
                                           SingingClip *clip, Track *track) {
    const auto a = new EditSingingClipPropertiesAction;
    a->m_oldArgs = oldArgs;
    a->m_newArgs = newArgs;
    a->m_clip = clip;
    a->m_track = track;
    return a;
}

void EditSingingClipPropertiesAction::execute() {
    m_track->removeClip(m_clip);
    auto newArgs = m_newArgs;
    // opendspx requires clip.pos = start + clipStart to stay non-negative.
    newArgs.start = std::max(newArgs.start, -newArgs.clipStart);
    m_clip->setName(newArgs.name);
    m_clip->setStart(newArgs.start);
    m_clip->setClipStart(newArgs.clipStart);
    m_clip->setLength(newArgs.length);
    m_clip->setClipLen(newArgs.clipLen);
    m_track->insertClip(m_clip);
    m_clip->notifyPropertyChanged();
}

void EditSingingClipPropertiesAction::undo() {
    m_track->removeClip(m_clip);
    m_clip->setName(m_oldArgs.name);
    m_clip->setStart(m_oldArgs.start);
    m_clip->setClipStart(m_oldArgs.clipStart);
    m_clip->setLength(m_oldArgs.length);
    m_clip->setClipLen(m_oldArgs.clipLen);
    m_track->insertClip(m_clip);
    m_clip->notifyPropertyChanged();
}