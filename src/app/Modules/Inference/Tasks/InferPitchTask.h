//
// Created by fluty on 24-10-2.
//

#ifndef INFERPITCHTASK_H
#define INFERPITCHTASK_H

#include <synthrt/SVS/Inference.h>

#include "IInferTask.h"
#include "Modules/Inference/Models/GenericInferModel.h"
#include "Modules/Inference/Models/InferInputBase.h"
#include "Modules/Inference/Models/InferInputNote.h"
#include "Modules/Inference/Models/InferParamCurve.h"
#include "Modules/Inference/Models/SingerIdentifier.h"

class InferPitchTask final : public IInferTask {
    Q_OBJECT

public:
    class InferPitchInput {
    public:
        INFER_INPUT_COMMON_MEMBERS
        InferParamCurve expressiveness;

        bool operator==(const InferPitchInput &other) const;
    };

    [[nodiscard]] int clipId() const override;
    [[nodiscard]] int pieceId() const override;
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
    GenericInferModel buildInputJson() const;
    bool processOutput(const GenericInferModel &model);

    srt::NO<srt::Inference> m_inferencePitch;
    QString m_previewText;
    InferPitchInput m_input;
    InferParamCurve m_result;
    QString m_inputHash;
    bool m_success = false;
};



#endif //INFERPITCHTASK_H
