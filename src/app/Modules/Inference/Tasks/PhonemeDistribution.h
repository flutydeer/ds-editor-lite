#ifndef PHONEMEDISTRIBUTION_H
#define PHONEMEDISTRIBUTION_H

#include "Modules/Inference/Models/NoteInferenceSnapshot.h"
#include "Modules/Inference/Models/PhonemeNameResult.h"

void distributePhonemesToNotes(const QList<NoteInferenceSnapshot> &notes,
                               QList<PhonemeNameResult> &results, int gapThresholdTicks);

#endif // PHONEMEDISTRIBUTION_H
