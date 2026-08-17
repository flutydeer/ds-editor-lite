#ifndef SYLLABIFICATION_H
#define SYLLABIFICATION_H

#include "Modules/Inference/Models/NoteInferenceSnapshot.h"
#include "Modules/Inference/Models/PhonemeNameResult.h"

#include <QStringList>

class InferInputNote;
class Timeline;

namespace Syllabification {
    struct PhonemeRange {
        int start = 0;
        int count = 0;
    };

    bool isSyllabificationLyric(const QString &lyric);
    QList<PhonemeRange> phonemeRangesForNotes(const QStringList &lyrics,
                                              const QList<PhonemeName> &phonemes);
    void keepPhonemesOnWordRoots(const QList<NoteInferenceSnapshot> &notes,
                                 QList<PhonemeNameResult> &results);
    void distributeForInference(const QStringList &lyrics, QList<InferInputNote> &notes,
                                const Timeline &timeline, int clipStartTick);
    QList<QList<int>> collectForStorage(const QStringList &lyrics,
                                        const QList<InferInputNote> &notes,
                                        const Timeline &timeline, int clipStartTick);
}

#endif // SYLLABIFICATION_H
