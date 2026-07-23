//
// Created by fluty on 24-10-2.
//

#ifndef INFERPITCHTASK_H
#define INFERPITCHTASK_H

#include <atomic>

#include <synthrt/SVS/Inference.h>

#include "IInferTask.h"
#include "InferTaskCommon.h"
#include "Modules/Inference/Models/GenericInferModel.h"
#include "Modules/Inference/Models/InferInputBase.h"
#include "Modules/Inference/Models/InferInputNote.h"
#include "Modules/Inference/Models/InferParamCurve.h"
#include "Model/AppModel/SingerIdentifier.h"

class InferPitchTask final : public IInferTask {
    Q_OBJECT

public:
    class InferPitchInput : public InferInputBase {
    public:
        InferParamCurve expressiveness;

        bool operator==(const InferPitchInput &other) const;
        [[nodiscard]] QString semanticSignature() const;
        [[nodiscard]] GenericInferModel toEngineModel() const;
    };

    [[nodiscard]] int clipId() const override;
    [[nodiscard]] int pieceId() const override;
    [[nodiscard]] InferenceTaskContext inferenceContext() const override;
    [[nodiscard]] bool success() const override;

    explicit InferPitchTask(InferPitchInput input);
    InferPitchInput input() const;
    InferParamCurve result();

private:
    void runTask() override;
    bool runInference(const GenericInferModel &model, InferParam &outPitch, QString &error);
    void terminate() override;
    void abort();
    void buildPreviewText();
    bool processOutput(const GenericInferModel &model);

    QString m_previewText;
    InferPitchInput m_input;
    InferParamCurve m_result;
    QString m_inputHash;
    std::atomic<bool> m_success{false};
    ActiveInference m_activeInference;
};



#endif // INFERPITCHTASK_H
