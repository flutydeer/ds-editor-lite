//
// Created by fluty on 24-9-15.
//

#ifndef IINFERPIECE_H
#define IINFERPIECE_H

#include "Utils/Macros.h"

interface IInferPiece : public UniqueObject {
    I_DECL(IInferPiece)
    I_NODSCD(int clipId() const);
    I_NODSCD(int noteStartTick() const);
    I_NODSCD(int noteEndTick() const);
};

#endif // IINFERPIECE_H
