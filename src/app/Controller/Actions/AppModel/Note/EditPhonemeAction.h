//
// Created by fluty on 24-2-14.
//

#ifndef EDITPHONEMEACTION_H
#define EDITPHONEMEACTION_H

#include "Model/Note.h"
#include "Modules/History/IAction.h"

class SingingClip;
class Note;

class EditPhonemeAction final : public IAction {
public:
    static EditPhonemeAction *build(Note *note, const Phoneme &phoneme);
    void execute() override;
    void undo() override;

private:
    Note *m_note = nullptr;
    Phoneme m_phoneme;
    QList<Phoneme> m_phonemes;
};



#endif //EDITPHONEMEACTION_H
