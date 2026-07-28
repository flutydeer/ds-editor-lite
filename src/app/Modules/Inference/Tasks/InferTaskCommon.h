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

#include <diffsinger/Infer/dsinfer/Api/Inferences/Common/1/CommonApiL1.h>
#include <diffsinger/Infer/dsinfer/Api/Inferences/Acoustic/1/AcousticApiL1.h>

#include <diffsinger/Session/ModelSetHandle.h>

class InferWord;
class InferParam;
struct InferSpeakerMix;

class ActiveInference final {
public:
    /// Model — the {inference, importOptions} pair a DiffSinger task consumes.
    /// B1b: previously a nested type of SingerModelSession; now defined locally
    /// since ActiveInference adapts a ds::session::ModelSetHandle into this
    /// shape via handle->load(kind) + handle->stages().find(kind)->options.
    /// Field shape is unchanged so all 4 task call sites compile unchanged.
    struct Model {
        srt::core::NO<srt::svs::Inference> inference;
        srt::core::NO<srt::svs::InferenceImportOptions> importOptions;
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

    // B1b: acquire now takes a ModelSetHandle (from VoicebankSession::ensureModelSet)
    // instead of a SingerModelSession. Internally load(kind) + model(kind) +
    // stages().find(kind)->options build the equivalent {inference, importOptions}
    // pair. Handle's public interface is unchanged, so the 4 DiffSinger task
    // call sites continue to compile.
    srt::core::Expected<Handle> acquire(const std::shared_ptr<ds::session::ModelSetHandle> &handle,
                                        ds::infer::StageKind kind);
    void stop();

private:
    void clear(std::uint64_t generation);

    std::mutex m_mutex;
    srt::core::NO<srt::svs::Inference> m_inference;
    std::uint64_t m_generation = 0;
    bool m_stopRequested = false;
};

auto createParamInfo(std::string_view tag) -> srt::svs::Api::Common::L1::InputParameterInfo;

auto convertInputWords(const QList<InferWord> &words, const std::string &speakerName,
                       const InferSpeakerMix &speakerMix,
                       const std::map<std::string, std::string> &speakerMapping, QString &error)
    -> std::vector<srt::svs::Api::Common::L1::InputWordInfo>;

auto convertInputParams(const QList<InferParam> &params)
    -> std::vector<srt::svs::Api::Common::L1::InputParameterInfo>;

auto createStaticSpeaker(const std::string &speaker) -> srt::svs::Api::Common::L1::InputSpeakerInfo;

auto convertInputSpeakers(const InferSpeakerMix &speakerMix,
                          const std::map<std::string, std::string> &speakerMapping, QString &error)
    -> std::vector<srt::svs::Api::Common::L1::InputSpeakerInfo>;

#endif // INFERTASKCOMMON_H
