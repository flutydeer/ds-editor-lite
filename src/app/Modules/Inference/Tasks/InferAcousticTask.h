//
// Created by fluty on 24-10-7.
//

#ifndef INFERACOUSTICTASK_H
#define INFERACOUSTICTASK_H

#include <atomic>

#include <synthrt/SVS/Inference.h>

#include "IInferTask.h"
#include "InferTaskCommon.h"
#include "Modules/Inference/Models/GenericInferModel.h"
#include "Modules/Inference/Models/InferInputBase.h"
#include "Modules/Inference/Models/InferParamCurve.h"
#include "Modules/Inference/Models/SingerIdentifier.h"

class InferInputNote;

class InferAcousticTask final : public IInferTask {
    Q_OBJECT

public:
    class InferAcousticInput : public InferInputBase {
    public:
        InferParamCurve pitch;
        InferParamCurve breathiness;
        InferParamCurve tension;
        InferParamCurve voicing;
        InferParamCurve energy;
        InferParamCurve mouthOpening;

        InferParamCurve gender;
        InferParamCurve velocity;
        InferParamCurve toneShift;

        bool operator==(const InferAcousticInput &other) const;
        [[nodiscard]] QString semanticSignature() const;
        [[nodiscard]] GenericInferModel toEngineModel() const;
    };

    [[nodiscard]] int clipId() const override;
    [[nodiscard]] int pieceId() const override;
    [[nodiscard]] InferenceTaskContext inferenceContext() const override;
    [[nodiscard]] bool success() const override;

    explicit InferAcousticTask(InferAcousticInput input);
    InferAcousticInput input() const;
    QString result() const;

private:
    void runTask() override;
    bool runInference(const GenericInferModel &model, const QString &outputPath, QString &error);
    void terminate() override;
    void abort();
    void buildPreviewText();

    QString m_previewText;
    InferAcousticInput m_input;
    QString m_result;
    QString m_inputHash;
    std::atomic<bool> m_success{false};
    ActiveInference m_activeInference;
};



#endif // INFERACOUSTICTASK_H
