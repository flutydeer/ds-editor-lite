//
// Created by fluty on 2024/2/8.
//

#include "EditSingingClipPropertiesAction.h"

#include "Model/Track.h"
#include "Model/Note.h"

EditSingingClipPropertiesAction *
    EditSingingClipPropertiesAction::build(const Clip::ClipCommonProperties &oldArgs,
                                           const Clip::ClipCommonProperties &newArgs,
                                           SingingClip *clip) {
    auto a = new EditSingingClipPropertiesAction;
    a->m_oldArgs = oldArgs;
    a->m_newArgs = newArgs;
    a->m_clip = clip;
    return a;
}
void EditSingingClipPropertiesAction::execute() {
    auto deltaTick = m_newArgs.start - m_oldArgs.start;
    if (deltaTick != 0)
        for (auto note : m_clip->notes()) {
            note->setStart(note->start() + deltaTick);
            note->notifyPropertyChanged(Note::TimeAndKey);
        }

    m_clip->setName(m_newArgs.name);
    m_clip->setStart(m_newArgs.start);
    m_clip->setClipStart(m_newArgs.clipStart);
    m_clip->setLength(m_newArgs.length);
    m_clip->setClipLen(m_newArgs.clipLen);
    m_clip->notifyPropertyChanged();
}
void EditSingingClipPropertiesAction::undo() {
    auto deltaTick = m_newArgs.start - m_oldArgs.start;
    if (deltaTick != 0)
        for (auto note : m_clip->notes()) {
            note->setStart(note->start() - deltaTick);
            note->notifyPropertyChanged(Note::TimeAndKey);
        }

    m_clip->setName(m_oldArgs.name);
    m_clip->setStart(m_oldArgs.start);
    m_clip->setClipStart(m_oldArgs.clipStart);
    m_clip->setLength(m_oldArgs.length);
    m_clip->setClipLen(m_oldArgs.clipLen);
    m_clip->notifyPropertyChanged();
}