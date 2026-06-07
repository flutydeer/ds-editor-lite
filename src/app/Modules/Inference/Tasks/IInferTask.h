//
// Created by fluty on 24-10-2.
//

#ifndef IINFERTASK_H
#define IINFERTASK_H

#include "Modules/Task/Task.h"
#include "Modules/Inference/Models/InferenceTaskContext.h"
#include "Utils/Macros.h"

LITE_INTERFACE IInferTask : public Task {
    I_DECL(IInferTask)
    I_NODSCD(int clipId() const);
    I_NODSCD(int pieceId() const);
    I_NODSCD(InferenceTaskContext inferenceContext() const);
    I_NODSCD(bool success() const);
};



#endif // IINFERTASK_H
