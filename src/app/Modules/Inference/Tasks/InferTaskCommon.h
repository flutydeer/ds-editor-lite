#ifndef INFERTASKCOMMON_H
#define INFERTASKCOMMON_H

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <QList>
#include <QString>

#include <dsinfer/Api/Inferences/Common/1/CommonApiL1.h>
#include <dsinfer/Api/Inferences/Acoustic/1/AcousticApiL1.h>

#include <synthrt/SVS/InferenceExecutive.h>

#include <lite/SynthrtEngine/SingerPipeline.h>

class InferWord;
class InferParam;
struct InferSpeakerMix;

/// One of the five stages of a singer.
using InferStage = lite::synthrt::SingerStage;

class ActiveInference final {
public:
    /// Inputs that a task requires to run one stage: the executive and the import options of
    /// the singer for that stage.
    ///
    /// The two members have different sources and are independent. The executive belongs to the
    /// pipeline, which builds it once per singer and shares it. The options are the import entry
    /// of the singer, which contains the speaker mapping.
    ///
    /// \a executive has the base type because one member represents five stage types. The
    /// requested stage determines the dynamic type, and the caller casts only to the executive
    /// type of the requested stage.
    struct Model {
        srt::InferenceExecutive *executive = nullptr;
        const srt::ContribImportOptions *importOptions = nullptr;
    };

    class Handle final {
    public:
        Handle(ActiveInference &owner, Model model, std::uint64_t generation);
        ~Handle();

        Handle(const Handle &) = delete;
        Handle &operator=(const Handle &) = delete;
        Handle(Handle &&other) noexcept;
        Handle &operator=(Handle &&) = delete;

        Model &model() noexcept;

    private:
        ActiveInference *m_owner;
        Model m_model;
        std::uint64_t m_generation;
    };

    /// Opens one stage of \a pipeline and holds it, so that stop() can reach it.
    ///
    /// Opening is expensive, and the pipeline caches the opened stage. A second request for the
    /// same stage therefore only returns a pointer.
    srt::Expected<Handle> acquire(lite::synthrt::SingerPipeline &pipeline, InferStage stage);
    void stop();

private:
    void clear(std::uint64_t generation);

    std::mutex m_mutex;
    srt::InferenceExecutive *m_executive = nullptr;
    std::uint64_t m_generation = 0;
    bool m_stopRequested = false;
};

auto createParamInfo(std::string_view tag) -> ds::Api::Common::L1::InputParameterInfo;

auto convertInputWords(const QList<InferWord> &words, const std::string &speakerName,
                       const InferSpeakerMix &speakerMix,
                       const std::map<std::string, std::string> &speakerMapping, QString &error)
    -> std::vector<ds::Api::Common::L1::InputWordInfo>;

// Serializes DirectML driver-facing inference and session lifecycle operations.
// Construct it before any pipeline or executive reference so their destruction
// also completes before the guard unlocks. It is a no-op for other providers.
class InferDirectMLSerializationGuard final {
public:
    InferDirectMLSerializationGuard();
    ~InferDirectMLSerializationGuard();

    InferDirectMLSerializationGuard(const InferDirectMLSerializationGuard &) = delete;
    InferDirectMLSerializationGuard &operator=(const InferDirectMLSerializationGuard &) = delete;

private:
    bool m_locked = false;
};

auto convertInputParams(const QList<InferParam> &params)
    -> std::vector<ds::Api::Common::L1::InputParameterInfo>;

auto createStaticSpeaker(const std::string &speaker) -> ds::Api::Common::L1::InputSpeakerInfo;

auto convertInputSpeakers(const InferSpeakerMix &speakerMix,
                          const std::map<std::string, std::string> &speakerMapping, QString &error)
    -> std::vector<ds::Api::Common::L1::InputSpeakerInfo>;

#endif // INFERTASKCOMMON_H
