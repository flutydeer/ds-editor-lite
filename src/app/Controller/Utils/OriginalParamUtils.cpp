//
// Created by fluty on 24-9-6.
//

#include "OriginalParamUtils.h"

#include "Model/AppModel/Clip.h"

void OriginalParamUtils::updateNotesPronunciation(const QList<Note *> &notes,
                                                  const QList<QString> &args, SingingClip *clip) {
    if (notes.count() != args.count()) {
        qFatal() << "updateNotesPronunciation() note count != args count:" << notes.count()
                 << args.count();
        return;
    }
    int i = 0;
    for (const auto note : notes) {
        note->setPronunciation(Note::Original, args[i]);
        i++;
    }
    clip->notifyNoteChanged(SingingClip::OriginalWordPropertyChange, notes);
}

void OriginalParamUtils::updateNotesPhonemeName(const QList<Note *> &notes,
                                                const QList<PhonemeNameResult> &args,
                                                SingingClip *clip) {
    if (notes.count() != args.count()) {
        qFatal() << "updateNotesPhonemeName() note count != args count:" << notes.count()
                 << args.count();
        return;
    }
    int i = 0;
    for (const auto note : notes) {
        note->setPhonemeNameInfo(PhonemeInfoSeperated::Ahead, Note::Original, args[i].aheadNames);
        note->setPhonemeNameInfo(PhonemeInfoSeperated::Normal, Note::Original, args[i].normalNames);
        note->setPhonemeNameInfo(PhonemeInfoSeperated::Final, Note::Original, args[i].finalNames);
        i++;
    }
    clip->notifyNoteChanged(SingingClip::OriginalWordPropertyChange, notes);
}

void OriginalParamUtils::updateNotesPhonemeOffset(const QList<Note *> &notes,
                                                  const QList<InferDurNote> &args,
                                                  SingingClip *clip) {
    if (notes.count() != args.count()) {
        qFatal() << "updateNotesPhonemeName() note count != args count:" << notes.count()
                 << args.count();
        return;
    }
    int i = 0;
    for (const auto note : notes) {
        note->setPhonemeOffsetInfo(PhonemeInfoSeperated::Ahead, Note::Original, args[i].aheadOffsets);
        note->setPhonemeOffsetInfo(PhonemeInfoSeperated::Normal, Note::Original, args[i].normalOffsets);
        note->setPhonemeOffsetInfo(PhonemeInfoSeperated::Final, Note::Original, args[i].finalOffsets);
        i++;
    }
    clip->notifyNoteChanged(SingingClip::OriginalWordPropertyChange, notes);
}