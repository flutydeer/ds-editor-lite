//
// Created by fluty on 24-9-6.
//

#include "OriginalParamUtils.h"

#include "Model/AppModel/Clip.h"

void OriginalParamUtils::updateNotePronPhoneme(const QList<Note *> &notes,
                                               const QList<Note::WordProperties> &args,
                                               SingingClip *clip) {
    int i = 0;
    for (const auto note : notes) {
        note->setPhonemeInfo(Note::Original, args[i].phonemes.original);
        note->setPronunciation(Note::Original, args[i].pronunciation.original);
        note->setPronCandidates(args[i].pronCandidates);
        i++;
    }
    clip->notifyNoteChanged(SingingClip::OriginalWordPropertyChange, notes);
}