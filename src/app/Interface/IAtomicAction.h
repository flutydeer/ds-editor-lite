//
// Created by fluty on 24-9-14.
//

#ifndef IATOMICACTION_H
#define IATOMICACTION_H

#include "Utils/Macros.h"

interface IAtomicAction {
    I_DECL(IAtomicAction)
    I_METHOD(void cancelAction()); // Revert to original state
    I_METHOD(void commitAction()); // Commit current action
    I_MEMBER(bool cancelRequested = false);
};

#endif // IATOMICACTION_H
