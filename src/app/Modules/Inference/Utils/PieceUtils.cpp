//
// Created by FlutyDeer on 2026/2/5.
//

#include "PieceUtils.h"

#include "Model/AppModel/InferPiece.h"
#include "Modules/SingingClipSlicer/Models/SliceResult.h"

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
