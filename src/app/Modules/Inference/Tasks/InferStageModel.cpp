#include "InferStageModel.h"

#include "Modules/Inference/Models/GenericInferModel.h"

#include <QDebug>

#include <utility>

InferStageModel::InferStageModel(std::shared_ptr<InferEngine::SingerPipelineLease> lease,
                                 ActiveInference::Handle handle)
    : m_lease(std::move(lease)), m_handle(std::move(handle)), m_model(m_handle->model()) {
}

std::optional<InferStageModel> InferStageModel::acquire(ActiveInference &activeInference,
                                                        const SingerIdentifier &identifier,
                                                        const InferStage stage,
                                                        const char *taskName, QString &error) {
    if (!inferEngine->initialized()) {
        qCritical().noquote() << taskName << ": the inference engine is not initialized";
        return std::nullopt;
    }
    auto lease = inferEngine->acquireSingerSession(identifier);
    if (!lease || !lease->pipeline()) {
        qCritical().noquote() << taskName << ": failed to acquire the singer session for"
                              << identifier;
        return std::nullopt;
    }
    return acquire(activeInference, std::move(lease), identifier, stage, taskName, error);
}

std::optional<InferStageModel>
    InferStageModel::acquire(ActiveInference &activeInference,
                             std::shared_ptr<InferEngine::SingerPipelineLease> lease,
                             const SingerIdentifier &identifier, const InferStage stage,
                             const char *taskName, QString &error) {
    if (!lease || !lease->pipeline())
        return std::nullopt;
    auto opened = activeInference.acquire(*lease->pipeline(), stage);
    if (!opened) {
        const auto role = lite::synthrt::roleOf(stage);
        qCritical().noquote().nospace()
            << taskName << ": failed to load the model of "
            << QString::fromUtf8(role.data(), static_cast<qsizetype>(role.size())) << " for "
            << identifier << ": " << QString::fromUtf8(opened.error().message());
        error = QString::fromUtf8(opened.error().toString());
        return std::nullopt;
    }
    auto handle = opened.take();
    if (!handle.model().executive) {
        qCritical().noquote() << taskName << ": the opened stage has no executive for"
                              << identifier;
        return std::nullopt;
    }
    return InferStageModel(std::move(lease), std::move(handle));
}

bool convertStageWords(const GenericInferModel &model,
                       const std::map<std::string, std::string> &speakerMapping,
                       const bool withSpeakers,
                       std::vector<ds::Api::Common::L1::InputWordInfo> &words,
                       std::vector<ds::Api::Common::L1::InputSpeakerInfo> &speakers,
                       QString &error) {
    words = convertInputWords(model.words, model.speaker.toStdString(), model.speakerMix,
                              speakerMapping, error);
    if (!error.isEmpty())
        return false;
    if (withSpeakers) {
        speakers = convertInputSpeakers(model.speakerMix, speakerMapping, error);
        if (!error.isEmpty())
            return false;
    }
    return true;
}
