//
// Created by fluty on 24-9-6.
//

#ifndef ORIGINALPARAMUTILS_H
#define ORIGINALPARAMUTILS_H

#include <QList>

class Note;
class SingingClip;
class InferDurPitNote;
class PhonemeNameResult;
class OriginalParamUtils {
public:
    static void updateNotesPronunciation(const QList<Note *> &notes, const QList<QString> &args,
                                         SingingClip *clip);
    static void updateNotesPhonemeName(const QList<Note *> &notes,
                                       const QList<PhonemeNameResult> &args, SingingClip *clip);
    static void updateNotesPhonemeOffset(const QList<Note *> &notes,
                                         const QList<InferDurPitNote> &args, SingingClip *clip);
};

#endif // ORIGINALPARAMUTILS_H
