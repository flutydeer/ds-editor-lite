#ifndef INFERSTAGEMODEL_H
#define INFERSTAGEMODEL_H

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <QString>

#include <dsinfer/Api/Inferences/Common/1/CommonApiL1.h>

#include "Modules/Inference/InferEngine.h"
#include "InferTaskCommon.h"

class GenericInferModel;

/// Own one stage for an inference run. Destroy its active handle before the pipeline lease.
/// Construct InferDirectMLSerializationGuard before acquiring the stage so teardown stays
/// serialized.
class InferStageModel final {
public:
    /// Acquire a stage or return no value, logging the task name and filling error when supplied.
    static std::optional<InferStageModel> acquire(ActiveInference &activeInference,
                                                  const SingerIdentifier &identifier,
                                                  InferStage stage, const char *taskName,
                                                  QString &error);

    /// Acquire another stage from the same pipeline lease.
    static std::optional<InferStageModel>
        acquire(ActiveInference &activeInference,
                std::shared_ptr<InferEngine::SingerPipelineLease> lease,
                const SingerIdentifier &identifier, InferStage stage, const char *taskName,
                QString &error);

    const std::shared_ptr<InferEngine::SingerPipelineLease> &lease() const {
        return m_lease;
    }

    /// Cast to the executive type matching the acquired stage.
    template <class Executive>
    Executive *executive() const {
        return static_cast<Executive *>(m_model.executive);
    }

    /// Absent import options mean identity mapping; a present value of the wrong type is an error.
    template <class ImportOptions>
    std::optional<std::map<std::string, std::string>> speakerMapping() const {
        const auto *options = m_model.importOptions;
        if (!options)
            return std::map<std::string, std::string>{};
        const auto *typed = options->as<ImportOptions>();
        if (!typed)
            return std::nullopt;
        return typed->speakerMapping;
    }

private:
    InferStageModel(std::shared_ptr<InferEngine::SingerPipelineLease> lease,
                    ActiveInference::Handle handle);

    std::shared_ptr<InferEngine::SingerPipelineLease> m_lease;
    std::optional<ActiveInference::Handle> m_handle;
    ActiveInference::Model m_model;
};

/// Convert input words and optionally frame speakers using the singer-to-model mapping.
/// Return false and set error on failure.
bool convertStageWords(const GenericInferModel &model,
                       const std::map<std::string, std::string> &speakerMapping, bool withSpeakers,
                       std::vector<ds::Api::Common::L1::InputWordInfo> &words,
                       std::vector<ds::Api::Common::L1::InputSpeakerInfo> &speakers,
                       QString &error);

#endif // INFERSTAGEMODEL_H
