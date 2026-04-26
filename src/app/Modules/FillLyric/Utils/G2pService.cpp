#include "Modules/FillLyric/Utils/G2pService.h"

#include <LangCore/Core/Manager.h>
#include <QMap>

#include "Modules/FillLyric/Utils/TextTagger.h"

namespace FillLyric
{
    QList<G2pResult> G2pService::convert(const QList<LangNote> &notes, const std::vector<std::string> &priorityG2pIds,
                                         const QMap<std::string, std::string> &langToG2pId) {

        std::vector<std::string> taggerInput;
        for (const auto &note : notes)
            taggerInput.push_back(note.lyric.toStdString());

        const auto taggerRes = TextTagger::tag(taggerInput, false, priorityG2pIds);

        if (static_cast<int>(taggerRes.size()) != notes.size())
            return {};

        const auto langMgr = LangCore::Manager::instance();
        if (!langMgr)
            return {};

        std::vector<std::unique_ptr<LangCore::G2pInput>> g2pInputs;
        for (int i = 0; i < notes.size(); i++) {
            const auto language = notes[i].language == QStringLiteral("unknown") ? taggerRes[i].language
                                                                                 : notes[i].language.toStdString();
            std::string g2pId;
            if (notes[i].g2pId != QStringLiteral("unknown") && notes[i].g2pId != QStringLiteral("slur")
                && !notes[i].g2pId.isEmpty()) {
                g2pId = notes[i].g2pId.toStdString();
            } else {
                g2pId = langToG2pId.value(language, "g2p-" + language + "-official");
            }
            g2pInputs.push_back(std::make_unique<LangCore::G2pInput>(taggerRes[i].lyric, g2pId));
        }

        std::vector<LangCore::G2pInput *> g2pInputPtrs;
        for (const auto &ptr : g2pInputs)
            g2pInputPtrs.push_back(ptr.get());

        const auto g2pRes = langMgr->convert(g2pInputPtrs);

        QList<G2pResult> results;
        const int resultCount = qMin(static_cast<int>(g2pRes.size()), static_cast<int>(taggerRes.size()));
        for (int i = 0; i < resultCount; i++) {
            G2pResult result;
            const auto language = notes[i].language == QStringLiteral("unknown") ? taggerRes[i].language
                                                                                  : notes[i].language.toStdString();
            result.language = QString::fromStdString(language);
            result.g2pId = QString::fromStdString(langToG2pId.value(language, "g2p-" + language + "-official"));
            result.syllable = g2pRes[i].pronunciation.c_str();
            for (const auto &candidate : g2pRes[i].candidates)
                result.candidates.push_back(candidate.c_str());
            results.append(result);
        }

        return results;
    }

    G2pResult G2pService::convertSingle(const LangNote &note, const std::vector<std::string> &priorityG2pIds,
                                        const QMap<std::string, std::string> &langToG2pId) {
        const auto results = convert({note}, priorityG2pIds, langToG2pId);
        if (results.isEmpty())
            return {};
        return results.first();
    }
} // namespace FillLyric
