//
// Created by fluty on 24-10-7.
//

#ifndef INFERACOUSTICTASK_H
#define INFERACOUSTICTASK_H

#include "IInferTask.h"
#include "Modules/Inference/Models/GenericInferModel.h"
#include "Modules/Inference/Models/InferInputBase.h"
#include "Modules/Inference/Models/InferParamCurve.h"

class InferInputNote;

class InferAcousticTask final : public IInferTask {
    Q_OBJECT

public:
    class InferAcousticInput {
    public:
        INFER_INPUT_COMMON_MEMBERS
        InferParamCurve pitch;
        InferParamCurve breathiness;
        InferParamCurve tension;
        InferParamCurve voicing;
        InferParamCurve energy;

        InferParamCurve gender;
        InferParamCurve velocity;

        bool operator==(const InferAcousticInput &other) const;
    };

    [[nodiscard]] int clipId() const override;
    [[nodiscard]] int pieceId() const override;
    [[nodiscard]] bool success() const override;

    explicit InferAcousticTask(InferAcousticInput input);
    InferAcousticInput input() const;
    QString result() const;

private:
    void runTask() override;
    void terminate() override;
    void abort();
    void buildPreviewText();
    GenericInferModel buildInputJson() const;
    bool processOutput(const GenericInferModel &model);

    QString m_previewText;
    InferAcousticInput m_input;
    QString m_result;
    QString m_inputHash;
    bool m_success = false;
};



#endif // INFERACOUSTICTASK_H
