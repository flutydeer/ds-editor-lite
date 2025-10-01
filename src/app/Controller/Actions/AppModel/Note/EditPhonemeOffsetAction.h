//
// Created by fluty on 24-9-11.
//

#ifndef EDITPHONEMEOFFSETACTION_H
#define EDITPHONEMEOFFSETACTION_H

#include "Model/AppModel/Note.h"
#include "Modules/History/IAction.h"

class SingingClip;

class EditPhonemeOffsetAction final : public IAction {
public:
    explicit EditPhonemeOffsetAction(Note *note, Phonemes::Type type, const QList<int> &offsets,
                                     SingingClip *clip);
    void execute() override;
    void undo() override;

private:
    Note *m_note;
    Phonemes::Type m_type;
    QList<int> m_oldOffsets;
    QList<int> m_newOffsets;
    SingingClip *m_clip;
};



#endif // EDITPHONEMEOFFSETACTION_H
