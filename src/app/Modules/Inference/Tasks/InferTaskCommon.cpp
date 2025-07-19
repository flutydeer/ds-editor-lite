#include "InferTaskCommon.h"
#include "Modules/Inference/Models/GenericInferModel.h"

namespace Co = ds::Api::Common::L1;
namespace Ac = ds::Api::Acoustic::L1;

auto createParamInfo(std::string_view tag) -> Co::InputParameterInfo {
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

auto convertInputWords(
    const QList<InferWord> &words, std::string speakerName) -> std::vector<Co::InputWordInfo> {

    std::vector<Co::InputWordInfo> inputWords;
    inputWords.reserve(words.size());

    for (const auto &word : std::as_const(words)) {
        std::vector<Co::InputNoteInfo> inputNotes;
        inputNotes.reserve(word.notes.size());
        for (const auto &note : std::as_const(word.notes)) {
            inputNotes.emplace_back(
                /* key */ note.key,
                /* cents */ note.cents,
                /* duration */ note.duration,
                /* glide */ note.glide == "up"
                             ? Co::GlideType::GT_Up
                             : (note.glide == "down"
                                    ? Co::GlideType::GT_Down
                                    : Co::GlideType::GT_None),
                /* is_rest */ note.is_rest
            );
        }

        std::vector<Co::InputPhonemeInfo> inputPhones;
        inputPhones.reserve(word.phones.size());
        for (const auto &phone : std::as_const(word.phones)) {
            inputPhones.emplace_back(
                /* token */ phone.token.toStdString(),
                /* language */ phone.languageDictId.toStdString(),
                /* tone */ 0,
                /* start */ phone.start,
                /* speakers */ std::vector{Co::InputPhonemeInfo::Speaker{std::move(speakerName), 1}}
            );
        }
        inputWords.emplace_back(std::move(inputPhones), std::move(inputNotes));
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
        inputParams.emplace_back(std::move(inputParam));
    }
    return inputParams;
}

auto createStaticSpeaker(std::string speaker) -> Co::InputSpeakerInfo {
    Co::InputSpeakerInfo inputSpeaker;
    inputSpeaker.name = std::move(speaker);
    inputSpeaker.proportions = {1.0};
    inputSpeaker.interval = 0;
    return inputSpeaker;
}
