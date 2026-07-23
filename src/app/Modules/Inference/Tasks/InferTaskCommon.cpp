#include "InferTaskCommon.h"
#include "Modules/Inference/Models/GenericInferModel.h"
#include "Model/AppModel/InferSpeakerMix.h"

#include <utility>
#include <QCoreApplication>

namespace Co = srt::svs::Api::Common::L1;

namespace {
    bool mapSpeakerName(const std::string &speakerName,
                        const std::map<std::string, std::string> &speakerMapping,
                        std::string &mappedSpeakerName) {
        if (speakerMapping.empty()) {
            mappedSpeakerName = speakerName;
            return true;
        }

        if (const auto it = speakerMapping.find(speakerName); it != speakerMapping.end()) {
            mappedSpeakerName = it->second;
            qDebug() << "mapped speaker" << speakerName << "to" << mappedSpeakerName;
            return true;
        }
        return false;
    }
}

ActiveInference::Handle::Handle(ActiveInference &owner, SingerModelSession::Model model,
                                std::uint64_t generation)
    : m_owner(&owner), m_model(std::move(model)), m_generation(generation) {
}

ActiveInference::Handle::~Handle() {
    if (m_owner)
        m_owner->clear(m_generation);
}

ActiveInference::Handle::Handle(Handle &&other) noexcept
    : m_owner(std::exchange(other.m_owner, nullptr)), m_model(std::move(other.m_model)),
      m_generation(other.m_generation) {
}

SingerModelSession::Model &ActiveInference::Handle::model() noexcept {
    return m_model;
}

srt::core::Expected<ActiveInference::Handle>
    ActiveInference::acquire(const std::shared_ptr<SingerModelSession> &session,
                             ds::infer::StageKind kind) {
    auto result = session->acquire(kind);
    if (!result)
        return result.takeError();

    auto model = result.take();
    srt::core::NO<srt::svs::Inference> inferenceToStop;
    std::uint64_t generation;
    {
        std::lock_guard lock(m_mutex);
        m_inference = model.inference;
        generation = ++m_generation;
        if (m_stopRequested)
            inferenceToStop = m_inference;
    }
    if (inferenceToStop)
        inferenceToStop->stop();
    return Handle(*this, std::move(model), generation);
}

void ActiveInference::clear(std::uint64_t generation) {
    std::lock_guard lock(m_mutex);
    if (m_generation == generation)
        m_inference.reset();
}

void ActiveInference::stop() {
    srt::core::NO<srt::svs::Inference> inference;
    {
        std::lock_guard lock(m_mutex);
        m_stopRequested = true;
        inference = m_inference;
    }
    if (inference)
        inference->stop();
}

auto createParamInfo(const std::string_view tag) -> Co::InputParameterInfo {
    if (tag == Co::Tags::Pitch.name()) {
        return Co::InputParameterInfo{Co::Tags::Pitch};
    } else if (tag == Co::Tags::Breathiness.name()) {
        return Co::InputParameterInfo{Co::Tags::Breathiness};
    } else if (tag == Co::Tags::Energy.name()) {
        return Co::InputParameterInfo{Co::Tags::Energy};
    } else if (tag == Co::Tags::Gender.name()) {
        return Co::InputParameterInfo{Co::Tags::Gender};
    } else if (tag == Co::Tags::Tension.name()) {
        return Co::InputParameterInfo{Co::Tags::Tension};
    } else if (tag == Co::Tags::Velocity.name()) {
        return Co::InputParameterInfo{Co::Tags::Velocity};
    } else if (tag == Co::Tags::Voicing.name()) {
        return Co::InputParameterInfo{Co::Tags::Voicing};
    } else if (tag == Co::Tags::MouthOpening.name()) {
        return Co::InputParameterInfo{Co::Tags::MouthOpening};
    } else if (tag == Co::Tags::ToneShift.name()) {
        return Co::InputParameterInfo{Co::Tags::ToneShift};
    } else if (tag == Co::Tags::F0.name()) {
        return Co::InputParameterInfo{Co::Tags::F0};
    } else if (tag == Co::Tags::Expr.name()) {
        return Co::InputParameterInfo{Co::Tags::Expr};
    }
    return Co::InputParameterInfo{};
}

auto convertInputWords(const QList<InferWord> &words, const std::string &speakerName,
                       const InferSpeakerMix &speakerMix,
                       const std::map<std::string, std::string> &speakerMapping, QString &error)
    -> std::vector<Co::InputWordInfo> {

    // Pre-convert speakerMix sources → static per-phone speaker weights.
    // Duration has no frame-level speakers field (DurationStartInput only has
    // words + duration), so phoneme-level Speaker is the only mix path.
    // For stages with frame-level speakers (Pitch/Variance/Acoustic), the
    // frame-level InputSpeakerInfo takes precedence; per-phone speakers here
    // are still filled for consistency but may be overridden.
    // Duration does not support time curves: take the first proportion as a
    // static weight (empty proportions → 1.0).
    std::vector<std::pair<std::string, double>> staticMix;
    if (speakerMix.sources.isEmpty()) {
        // Fallback to single speaker (backward compatible).
        std::string mapped;
        if (!mapSpeakerName(speakerName, speakerMapping, mapped)) {
            error = QCoreApplication::translate(
                "InferTaskCommon", "Speaker mapping not found for speaker %1")
                        .arg(QString::fromStdString(speakerName));
            return {};
        }
        staticMix.emplace_back(std::move(mapped), 1.0);
    } else {
        for (const auto &source : std::as_const(speakerMix.sources)) {
            if (!source.isValid()) {
                error = QCoreApplication::translate("InferTaskCommon", "Invalid speaker mix source");
                return {};
            }
            std::string mapped;
            const auto srcName = source.speaker.toStdString();
            if (!mapSpeakerName(srcName, speakerMapping, mapped)) {
                error = QCoreApplication::translate(
                    "InferTaskCommon", "Speaker mapping not found for speaker %1")
                            .arg(source.speaker);
                return {};
            }
            const double p = source.proportions.isEmpty() ? 1.0 : source.proportions.first();
            staticMix.emplace_back(std::move(mapped), p);
        }
    }

    std::vector<Co::InputWordInfo> inputWords;
    inputWords.reserve(words.size());

    for (const auto &word : std::as_const(words)) {
        std::vector<Co::InputNoteInfo> inputNotes;
        inputNotes.reserve(word.notes.size());
        for (const auto &note : std::as_const(word.notes)) {
            inputNotes.emplace_back(
                Co::InputNoteInfo{/* key */ note.key,
                                  /* cents */ note.cents,
                                  /* duration */ note.duration,
                                  /* glide */ note.glide == "up" ? Co::GlideType::GT_Up
                                  : note.glide == "down"         ? Co::GlideType::GT_Down
                                                                 : Co::GlideType::GT_None,
                                  /* is_rest */ note.is_rest});
        }

        std::vector<Co::InputPhonemeInfo> inputPhones;
        inputPhones.reserve(word.phones.size());
        for (const auto &phone : std::as_const(word.phones)) {
            std::vector<Co::InputPhonemeInfo::Speaker> speakers;
            speakers.reserve(staticMix.size());
            for (const auto &[name, p] : staticMix) {
                speakers.emplace_back(Co::InputPhonemeInfo::Speaker{name, p});
            }
            inputPhones.emplace_back(Co::InputPhonemeInfo{
                /* token */ phone.token.toStdString(),
                /* language */ phone.languageDictId.toStdString(),
                /* tone */ phone.tone,
                /* start */ phone.start,
                /* speakers */ std::move(speakers)});
        }
        inputWords.emplace_back(Co::InputWordInfo{std::move(inputPhones), std::move(inputNotes)});
    }

    return inputWords;
}

auto convertInputParams(const QList<InferParam> &params) -> std::vector<Co::InputParameterInfo> {
    std::vector<Co::InputParameterInfo> inputParams;
    inputParams.reserve(params.size());
    for (const auto &param : std::as_const(params)) {
        auto inputParam = createParamInfo(param.tag.toStdString());
        if (inputParam.tag.name().empty()) {
            continue;
        }
        inputParam.interval = param.interval;
        inputParam.values = {param.values.begin(), param.values.end()};
        if (param.retake.end > param.retake.start) {
            inputParam.retake = Co::InputParameterInfo::RetakeRange{
                param.retake.start, param.retake.end};
        }
        inputParams.emplace_back(std::move(inputParam));
    }
    return inputParams;
}

auto createStaticSpeaker(const std::string &speaker) -> Co::InputSpeakerInfo {
    Co::InputSpeakerInfo inputSpeaker;
    inputSpeaker.name = std::move(speaker);
    inputSpeaker.proportions = {1.0};
    inputSpeaker.interval = 0;
    return inputSpeaker;
}

auto convertInputSpeakers(const InferSpeakerMix &speakerMix,
                          const std::map<std::string, std::string> &speakerMapping, QString &error)
    -> std::vector<Co::InputSpeakerInfo> {
    std::vector<Co::InputSpeakerInfo> inputSpeakers;
    inputSpeakers.reserve(speakerMix.sources.size());

    for (const auto &source : speakerMix.sources) {
        if (!source.isValid()) {
            error = QCoreApplication::translate("InferTaskCommon", "Invalid speaker mix source");
            return {};
        }

        std::string mappedSpeakerName;
        const auto speakerName = source.speaker.toStdString();
        if (!mapSpeakerName(speakerName, speakerMapping, mappedSpeakerName)) {
            error = QCoreApplication::translate("InferTaskCommon", "Speaker mapping not found for speaker %1").arg(source.speaker);
            return {};
        }

        Co::InputSpeakerInfo inputSpeaker;
        inputSpeaker.name = std::move(mappedSpeakerName);
        inputSpeaker.interval = source.interval;
        inputSpeaker.proportions.assign(source.proportions.begin(), source.proportions.end());
        inputSpeakers.emplace_back(std::move(inputSpeaker));
    }

    return inputSpeakers;
}