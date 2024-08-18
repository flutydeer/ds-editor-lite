//
// Created by fluty on 2024/2/8.
//

#include "EditSingingClipPropertiesAction.h"

#include "Model/AppModel/Note.h"
#include "Model/AppModel/Track.h"

EditSingingClipPropertiesAction *
    EditSingingClipPropertiesAction::build(const Clip::ClipCommonProperties &oldArgs,
                                           const Clip::ClipCommonProperties &newArgs,
                                           SingingClip *clip, Track *track) {
    auto a = new EditSingingClipPropertiesAction;
    a->m_oldArgs = oldArgs;
    a->m_newArgs = newArgs;
    a->m_clip = clip;
    a->m_track = track;
    return a;
}

void EditSingingClipPropertiesAction::execute() {
    // auto deltaTick = m_newArgs.start - m_oldArgs.start;
    // if (deltaTick != 0) {
    //     auto notes = m_clip->notes().toList();
    //     for (auto note : notes) {
    //         m_clip->removeNote(note);
    //         note->setStart(note->start() + deltaTick);
    //         m_clip->insertNote(note);
    //         note->notifyPropertyChanged(Note::TimeAndKey);
    //     }
    // }

    m_track->removeClip(m_clip);
    m_clip->setName(m_newArgs.name);
    m_clip->setStart(m_newArgs.start);
    m_clip->setClipStart(m_newArgs.clipStart);
    m_clip->setLength(m_newArgs.length);
    m_clip->setClipLen(m_newArgs.clipLen);
    m_track->insertClip(m_clip);
    m_clip->notifyPropertyChanged();
}

void EditSingingClipPropertiesAction::undo() {
    // auto deltaTick = m_newArgs.start - m_oldArgs.start;
    // if (deltaTick != 0) {
    //     auto notes = m_clip->notes().toList();
    //     for (auto note : notes) {
    //         m_clip->removeNote(note);
    //         note->setStart(note->start() - deltaTick);
    //         m_clip->insertNote(note);
    //         note->notifyPropertyChanged(Note::TimeAndKey);
    //     }
    // }

    m_track->removeClip(m_clip);
    m_clip->setName(m_oldArgs.name);
    m_clip->setStart(m_oldArgs.start);
    m_clip->setClipStart(m_oldArgs.clipStart);
    m_clip->setLength(m_oldArgs.length);
    m_clip->setClipLen(m_oldArgs.clipLen);
    m_track->insertClip(m_clip);
    m_clip->notifyPropertyChanged();
}