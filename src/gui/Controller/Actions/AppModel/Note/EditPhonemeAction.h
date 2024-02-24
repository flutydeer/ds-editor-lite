//
// Created by fluty on 24-2-14.
//

#ifndef EDITPHONEMEACTION_H
#define EDITPHONEMEACTION_H

#include "Controller/History/IAction.h"
#include "Model/Note.h"

class SingingClip;
class Note;

class EditPhonemeAction final : public IAction {
public:
    static EditPhonemeAction *build(Note *note, const Phoneme &phoneme, SingingClip *clip);
    void execute() override;
    void undo() override;

private:
    Note *m_note = nullptr;
    SingingClip *m_clip = nullptr;
    Phoneme m_phoneme;
    QList<Phoneme> m_phonemes;
};



#endif //EDITPHONEMEACTION_H
