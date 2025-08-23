//
// Created by fluty on 24-10-5.
//

#ifndef INFERVARIANCETASK_H
#define INFERVARIANCETASK_H

#include <synthrt/SVS/Inference.h>

#include "IInferTask.h"
#include "Modules/Inference/Models/GenericInferModel.h"
#include "Modules/Inference/Models/InferInputBase.h"
#include "Modules/Inference/Models/InferParamCurve.h"
#include "Modules/Inference/Models/SingerIdentifier.h"

class InferInputNote;

class InferVarianceTask final : public IInferTask {
    Q_OBJECT

public:
    class InferVarianceInput {
    public:
        INFER_INPUT_COMMON_MEMBERS
        InferParamCurve pitch;

        bool operator==(const InferVarianceInput &other) const;
    };

    class InferVarianceResult {
    public:
        InferParamCurve breathiness;
        InferParamCurve tension;
        InferParamCurve voicing;
        InferParamCurve energy;
        InferParamCurve mouthOpening;
    };

    [[nodiscard]] int clipId() const override;
    [[nodiscard]] int pieceId() const override;
    [[nodiscard]] bool success() const override;

    explicit InferVarianceTask(InferVarianceInput input);
    InferVarianceInput input() const;
    InferVarianceResult result() const;

private:
    void runTask() override;
    bool runInference(const GenericInferModel &model, QList<InferParam> &outParams, QString &error);
    void terminate() override;
    void abort();
    void buildPreviewText();
    GenericInferModel buildInputJson() const;
    bool processOutput(const GenericInferModel &model);

    srt::NO<srt::Inference> m_inferenceVariance;
    QString m_previewText;
    InferVarianceInput m_input;
    InferVarianceResult m_result;
    QString m_inputHash;
    bool m_success = false;
};

#endif // INFERVARIANCETASK_H
