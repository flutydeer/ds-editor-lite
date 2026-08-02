#include "PieceUtils.h"

#include <lite/ProjectModel/InferenceData/InferPiece.h>
#include <lite/ProjectModel/SingingClipSlicer/Models/SliceResult.h>

bool PieceUtils::isSamePiece(const InferPiece &left, const Segment &right) {
    if (left.notes.count() != right.notes.count())
        return false;

    if (!qFuzzyCompare(left.paddingStartMs, right.paddingStartMs))
        return false;

    if (!qFuzzyCompare(left.paddingEndMs, right.paddingEndMs))
        return false;

    for (int i = 0; i < left.notes.count(); i++) {
        if (left.notes[i] != right.notes[i])
            return false;
    }
    return true;
}
