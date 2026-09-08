#ifndef SYLLABIFICATION_H
#define SYLLABIFICATION_H

#include "Modules/Inference/Models/NoteInferenceSnapshot.h"
#include "Modules/Inference/Models/PhonemeNameResult.h"

#include <lite/ProjectModel/Utils/Syllabification.h>

class InferInputNote;
class Timeline;

namespace Syllabification {
    void keepPhonemesOnWordRoots(const QList<NoteInferenceSnapshot> &notes,
                                 QList<PhonemeNameResult> &results);
    void distributeForInference(const QStringList &lyrics, QList<InferInputNote> &notes,
                                const Timeline &timeline, int clipStartTick);
    QList<QList<int>> collectForStorage(const QStringList &lyrics,
                                        const QList<InferInputNote> &notes,
                                        const Timeline &timeline, int clipStartTick);
}

#endif // SYLLABIFICATION_H
