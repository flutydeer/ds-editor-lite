#ifndef DS_EDITOR_LITE_INFERENCEINPUTSIGNATURE_H
#define DS_EDITOR_LITE_INFERENCEINPUTSIGNATURE_H

#include <QString>

#include "Modules/Inference/Tasks/InferAcousticTask.h"
#include "Modules/Inference/Tasks/InferDurationTask.h"
#include "Modules/Inference/Tasks/InferPitchTask.h"
#include "Modules/Inference/Tasks/InferVarianceTask.h"

class InferPiece;
struct SingerIdentifier;

namespace InferenceInputSignature {
    [[nodiscard]] QString fromInput(const InferDurationTask::InferDurInput &input);
    [[nodiscard]] QString fromInput(const InferPitchTask::InferPitchInput &input);
    [[nodiscard]] QString fromInput(const InferVarianceTask::InferVarianceInput &input);
    [[nodiscard]] QString fromInput(const InferAcousticTask::InferAcousticInput &input);
    [[nodiscard]] QString fromCurrentPiece(const QString &taskType, const InferPiece &piece,
                                           const SingerIdentifier &identifier);
}

#endif // DS_EDITOR_LITE_INFERENCEINPUTSIGNATURE_H
