#ifndef INFERTASKCOMMON_H
#define INFERTASKCOMMON_H

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <QList>
#include <QString>

#include <dsinfer/Api/Inferences/Common/1/CommonApiL1.h>
#include <dsinfer/Api/Inferences/Acoustic/1/AcousticApiL1.h>

class InferWord;
class InferParam;
struct InferSpeakerMix;

auto createParamInfo(std::string_view tag) -> ds::Api::Common::L1::InputParameterInfo;

auto convertInputWords(const QList<InferWord> &words, const std::string &speakerName,
                       const std::map<std::string, std::string> &speakerMapping)
    -> std::vector<ds::Api::Common::L1::InputWordInfo>;

auto convertInputParams(const QList<InferParam> &params)
    -> std::vector<ds::Api::Common::L1::InputParameterInfo>;

auto createStaticSpeaker(const std::string &speaker) -> ds::Api::Common::L1::InputSpeakerInfo;

auto convertInputSpeakers(const InferSpeakerMix &speakerMix,
                          const std::map<std::string, std::string> &speakerMapping, QString &error)
    -> std::vector<ds::Api::Common::L1::InputSpeakerInfo>;

#endif // INFERTASKCOMMON_H
