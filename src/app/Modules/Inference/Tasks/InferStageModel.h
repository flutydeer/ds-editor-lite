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

/// Model of one stage of a singer, held for one inference run.
///
/// Encapsulates the steps that the four inference tasks share before building their stage input:
/// checking the engine, leasing the pipeline of the singer, opening the stage through
/// ActiveInference so that stop() can reach it, and reading the import options of the stage. The
/// lease is declared before the handle. The handle, which clears the executive from
/// ActiveInference, is therefore destroyed first, and the pipeline that owns the executive last.
///
/// The caller constructs its InferDirectMLSerializationGuard before calling acquire(), so that the
/// destruction of the stage also completes before the guard unlocks.
class InferStageModel final {
public:
    /// Acquires \a stage of the singer \a identifier. Returns no value if the engine is not
    /// initialized, the pipeline cannot be leased or the model cannot be opened. The reason is
    /// logged with \a taskName as its prefix and, if the engine reports an error, stored in
    /// \a error.
    static std::optional<InferStageModel> acquire(ActiveInference &activeInference,
                                                  const SingerIdentifier &identifier,
                                                  InferStage stage, const char *taskName,
                                                  QString &error);

    /// Acquires \a stage from the pipeline that \a lease already holds, so that two stages of one
    /// run, such as acoustic and vocoder, come from the same pipeline.
    static std::optional<InferStageModel>
        acquire(ActiveInference &activeInference,
                std::shared_ptr<InferEngine::SingerPipelineLease> lease,
                const SingerIdentifier &identifier, InferStage stage, const char *taskName,
                QString &error);

    /// Returns the lease of the pipeline from which the stage was acquired.
    const std::shared_ptr<InferEngine::SingerPipelineLease> &lease() const {
        return m_lease;
    }

    /// Returns the executive of the stage. The requested stage determines the dynamic type,
    /// because acquire() returns only the requested stage. The caller therefore casts to the
    /// executive type of the stage that it requested.
    template <class Executive>
    Executive *executive() const {
        return static_cast<Executive *>(m_model.executive);
    }

    /// Returns the speaker mapping of the import options of the stage, or no value if the singer
    /// declares no import options of the expected type. The caller specifies the import options
    /// type of its stage.
    template <class ImportOptions>
    std::optional<std::map<std::string, std::string>> speakerMapping() const {
        const auto *options = m_model.importOptions;
        const auto *typed = options ? options->as<ImportOptions>() : nullptr;
        if (!typed)
            return std::nullopt;
        return typed->speakerMapping;
    }

private:
    InferStageModel(std::shared_ptr<InferEngine::SingerPipelineLease> lease,
                    ActiveInference::Handle handle);

    std::shared_ptr<InferEngine::SingerPipelineLease> m_lease;
    std::optional<ActiveInference::Handle> m_handle;
    /// Executive and import options held by the handle, read once at construction.
    ActiveInference::Model m_model;
};

/// Converts the words of \a model, and its speakers if \a withSpeakers is true, with the speaker
/// names of the singer mapped through \a speakerMapping. Returns false and sets \a error on
/// failure. The duration stage has no frame-level speakers and passes \a withSpeakers as false.
bool convertStageWords(const GenericInferModel &model,
                       const std::map<std::string, std::string> &speakerMapping, bool withSpeakers,
                       std::vector<ds::Api::Common::L1::InputWordInfo> &words,
                       std::vector<ds::Api::Common::L1::InputSpeakerInfo> &speakers,
                       QString &error);

#endif // INFERSTAGEMODEL_H
