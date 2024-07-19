//
// Created by fluty on 2024/2/8.
//

#include "EditClipCommonPropertiesAction.h"

EditClipCommonPropertiesAction *
    EditClipCommonPropertiesAction::build(const Clip::ClipCommonProperties &oldArgs,
                                          const Clip::ClipCommonProperties &newArgs, Clip *clip) {
    auto a = new EditClipCommonPropertiesAction;
    a->m_oldArgs = oldArgs;
    a->m_newArgs = newArgs;
    a->m_clip = clip;
    return a;
}
void EditClipCommonPropertiesAction::execute() {
    m_clip->setName(m_newArgs.name);
    m_clip->setStart(m_newArgs.start);
    m_clip->setClipStart(m_newArgs.clipStart);
    m_clip->setLength(m_newArgs.length);
    m_clip->setClipLen(m_newArgs.clipLen);
    m_clip->notifyPropertyChanged();
}
void EditClipCommonPropertiesAction::undo() {
    m_clip->setName(m_oldArgs.name);
    m_clip->setStart(m_oldArgs.start);
    m_clip->setClipStart(m_oldArgs.clipStart);
    m_clip->setLength(m_oldArgs.length);
    m_clip->setClipLen(m_oldArgs.clipLen);
    m_clip->notifyPropertyChanged();
}