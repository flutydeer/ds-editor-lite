//
// Created by fluty on 2024/2/7.
//

#ifndef IACTION_H
#define IACTION_H

#include "Utils/Macros.h"

interface IAction {
    I_DECL(IAction)
    I_METHOD(void execute());
    I_METHOD(void undo());
};

#endif // IACTION_H
