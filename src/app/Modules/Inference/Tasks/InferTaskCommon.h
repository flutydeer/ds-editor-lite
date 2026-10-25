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

using InferStage = lite::synthrt::SingerStage;

/// Hold a stage for cancellation. A stopped start() can still return partial output;
/// consume it only when state() is srt::ITask::Succeeded.
class ActiveInference final {
public:
    /// Borrowed stage executive and import options, valid while the pipeline lease is held.
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

    /// Open or reuse a pipeline stage and register it for cancellation.
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
