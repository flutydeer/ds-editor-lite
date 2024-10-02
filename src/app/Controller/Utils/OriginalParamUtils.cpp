//
// Created by fluty on 24-9-6.
//

#include "OriginalParamUtils.h"

#include "Model/AppModel/Note.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/Inference/InferDurPitNote.h"
#include "Model/Inference/PhonemeNameModel.h"

#include <QDebug>

void OriginalParamUtils::updatePronunciation(const QList<Note *> &notes, const QList<QString> &args,
                                             SingingClip *clip) {
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

void OriginalParamUtils::updatePhoneName(const QList<Note *> &notes,
                                         const QList<PhonemeNameResult> &args, SingingClip *clip) {
    if (notes.count() != args.count()) {
        qFatal() << "updateNotesPhonemeName() note count != args count:" << notes.count()
                 << args.count();
        return;
    }
    int i = 0;
    for (const auto note : notes) {
        note->setPhonemeNameInfo(Phonemes::Ahead, Note::Original, args[i].aheadNames);
        note->setPhonemeNameInfo(Phonemes::Normal, Note::Original, args[i].normalNames);
        i++;
    }
    clip->notifyNoteChanged(SingingClip::OriginalWordPropertyChange, notes);
}

void OriginalParamUtils::updatePhoneOffset(const QList<Note *> &notes,
                                           const QList<InferDurPitNote> &args, SingingClip *clip) {
    if (notes.count() != args.count()) {
        qFatal() << "updateNotesPhonemeName() note count != args count:" << notes.count()
                 << args.count();
        return;
    }
    int i = 0;
    for (const auto note : notes) {
        note->setPhonemeOffsetInfo(Phonemes::Ahead, Note::Original, args[i].aheadOffsets);
        note->setPhonemeOffsetInfo(Phonemes::Normal, Note::Original, args[i].normalOffsets);
        i++;
    }
    clip->notifyNoteChanged(SingingClip::OriginalWordPropertyChange, notes);
}

void OriginalParamUtils::resetPhoneOffset(const QList<Note *> &notes, SingingClip *clip) {
    for (const auto note : notes) {
        note->setPhonemeOffsetInfo(Phonemes::Ahead, Note::Original, {});
        note->setPhonemeOffsetInfo(Phonemes::Normal, Note::Original, {});
    }
    clip->notifyNoteChanged(SingingClip::OriginalWordPropertyChange, notes);
}