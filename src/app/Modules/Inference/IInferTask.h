//
// Created by fluty on 24-10-2.
//

#ifndef IINFERTASK_H
#define IINFERTASK_H

#include "Modules/Task/Task.h"
#include "Utils/Macros.h"

interface IInferTask : public Task {
    I_DECL(IInferTask)
    I_NODSCD(int clipId() const);
    I_NODSCD(int pieceId() const);
    I_NODSCD(bool success() const);
};



#endif //IINFERTASK_H
