//
// Created by fluty on 24-10-2.
//

#ifndef INFERPITCHTASK_H
#define INFERPITCHTASK_H

#include "IInferTask.h"
#include "Model/Inference/InferDurPitNote.h"
#include "Model/Inference/InferParamCurve.h"

class InferPitchTask final : public IInferTask {
public:
    struct InferPitchInput {
        int clipId = -1;
        int pieceId = -1;
        QList<InferDurPitNote> notes;
        QString configPath;
        double tempo;
        InferParamCurve expressiveness;

        bool operator==(const InferPitchInput &other) const;
    };

    [[nodiscard]] int clipId() const override;
    [[nodiscard]] int pieceId() const override;
    [[nodiscard]] bool success() const override;

    explicit InferPitchTask(InferPitchInput input);
    InferPitchInput input();
    InferParamCurve result();

private:
    void runTask() override;
    void abort();
    void buildPreviewText();
    QString buildInputJson() const;
    bool processOutput(const QString &json);

    QString m_previewText;
    InferPitchInput m_input;
    InferParamCurve m_result;
    bool m_success = false;
};



#endif //INFERPITCHTASK_H
