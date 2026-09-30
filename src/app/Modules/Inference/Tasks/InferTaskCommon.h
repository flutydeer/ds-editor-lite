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

/// Which of a singer's five stages a task wants.
enum class InferStage {
    Duration,
    Pitch,
    Variance,
    Acoustic,
    Vocoder,
};

class ActiveInference final {
public:
    /// What a task needs to run one stage: the model, and what this singer asked of it.
    ///
    /// The two come from different places and neither implies the other. The executive is the
    /// pipeline's, built once per singer and shared; the options are the singer's own import
    /// entry, which is where a speaker mapping lives.
    ///
    /// \a executive is the base type because one member cannot be five types at once. The stage
    /// that was asked for decides which it really is, so a caller casts to the one it asked for
    /// and nothing else.
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

    /// Opens one stage of \a pipeline and keeps hold of it, so that stop() can reach it.
    ///
    /// Opening is the expensive part and the pipeline caches it, so asking twice for the same
    /// stage costs a pointer return.
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
