#ifndef INFERTASKCOMMON_H
#define INFERTASKCOMMON_H

#include <string_view>

#include <QList>

#include <dsinfer/Api/Inferences/Common/1/CommonApiL1.h>
#include <dsinfer/Api/Inferences/Acoustic/1/AcousticApiL1.h>

class InferWord;
class InferParam;

auto createParamInfo(
    std::string_view tag) -> ds::Api::Common::L1::InputParameterInfo;

auto convertInputWords(const QList<InferWord> &words,
                       std::string speakerName) -> std::vector<ds::Api::Common::L1::InputWordInfo>;

auto convertInputParams(
    const QList<InferParam> &params) -> std::vector<ds::Api::Common::L1::InputParameterInfo>;

auto createStaticSpeaker(std::string speaker) -> ds::Api::Common::L1::InputSpeakerInfo;

#endif // INFERTASKCOMMON_H