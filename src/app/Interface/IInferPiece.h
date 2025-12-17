//
// Created by fluty on 24-9-15.
//

#ifndef IINFERPIECE_H
#define IINFERPIECE_H

#include "Utils/Macros.h"
#include "Utils/UniqueObject.h"

LITE_INTERFACE IInferPiece : public UniqueObject {
    I_DECL(IInferPiece)
    I_NODSCD(int clipId() const);
};

#endif // IINFERPIECE_H
