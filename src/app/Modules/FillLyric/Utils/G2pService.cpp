#include "Modules/FillLyric/Utils/G2pService.h"

#include <QLoggingCategory>

#include <map>
#include <optional>

#include <synthrt/G2P/LanguageService.h>

#include "Modules/FillLyric/Utils/TextTagger.h"
#include "Modules/Language/G2pConvertRunner.h"
#include "Modules/Language/G2pInputAdapter.h"
Q_LOGGING_CATEGORY(logFillG2p, "fill.g2p")

namespace FillLyric {
    namespace {
        std::string toUtf8(const QString &value) {
            const auto bytes = value.toUtf8();
            return {bytes.constData(), static_cast<size_t>(bytes.size())};
        }

        QString fromUtf8(const std::string &value) {
            return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
        }
    }

    G2pService::G2pService(SingerIdentifier singer,
                           const srt::g2p::LanguageService &languageService)
        : m_singer(std::move(singer)), m_languageService(languageService) {
    }

    QList<G2pResult> G2pService::convert(const QList<LangNote> &notes,
                                         const std::vector<std::string> &priorityLanguages) const {

        // 预填充 results：所有 note 默认 copy fallback（pronunciation=lyric, candidates={lyric}）
        // 保证返回与输入等长，调用方不会越界
        QList<G2pResult> results;
        results.reserve(notes.size());
        for (const auto &note : notes) {
            G2pResult fallback;
            fallback.language = note.language;
            fallback.pronunciation = note.lyric;
            fallback.candidates = {note.lyric};
            results.append(fallback);
        }

        if (notes.isEmpty())
            return results;

        std::vector<std::string> taggerInput;
        taggerInput.reserve(notes.size());
        for (const auto &note : notes)
            taggerInput.push_back(toUtf8(note.lyric));

        const auto taggerRes = TextTagger::tag(taggerInput, false, priorityLanguages);

        if (m_singer.isEmpty()) {
            qCWarning(logFillG2p)
                << "Singer identifier is empty; using copy fallback for all notes";
            return results;
        }

        std::map<QString, std::optional<srt::g2p::LanguageRoute>> routeCache;
        std::vector<srt::g2p::G2pInput> requests;
        QList<int> requestIndices;
        const int commonCount = qMin(static_cast<int>(taggerRes.size()), notes.size());
        requests.reserve(commonCount);
        requestIndices.reserve(commonCount);
        for (int i = 0; i < commonCount; i++) {
            const auto language = notes[i].language == QStringLiteral("unknown")
                                      ? fromUtf8(taggerRes[i].language)
                                      : notes[i].language;
            auto routeIt = routeCache.find(language);
            if (routeIt == routeCache.end()) {
                const auto route = m_languageService.resolveLanguageRoute(
                    toUtf8(m_singer.packageId), toUtf8(m_singer.singerId), toUtf8(language));
                if (!route.hasValue()) {
                    qCWarning(logFillG2p) << "Failed to resolve G2P route for language" << language
                                          << ":" << fromUtf8(route.error().message());
                    routeIt = routeCache.emplace(language, std::nullopt).first;
                } else {
                    routeIt = routeCache.emplace(language, *route).first;
                }
            }
            if (!routeIt->second)
                continue;

            requests.push_back(G2pInputAdapter::fromRoute(taggerRes[i].lyric, *routeIt->second));
            requestIndices.append(i);
        }

        if (requests.empty())
            return results;

        const auto outcomes = G2pConvertRunner::convert(m_languageService, requests);

        // 调用方校验结果数量；可用结果按请求索引覆盖，其余保持 copy fallback。
        if (outcomes.size() != requests.size()) {
            qCWarning(logFillG2p) << "G2pConvertRunner returned" << outcomes.size()
                                  << "outcomes for" << requests.size() << "requests;"
                                  << "using copy fallback for unmatched notes";
        }

        const auto coveredCount = std::min(outcomes.size(), requests.size());
        for (size_t i = 0; i < coveredCount; i++) {
            const auto noteIdx = requestIndices.at(static_cast<qsizetype>(i));
            auto &result = results[noteIdx];

            result.language = notes[noteIdx].language == QStringLiteral("unknown")
                                  ? fromUtf8(taggerRes[noteIdx].language)
                                  : notes[noteIdx].language;

            const auto &outcome = outcomes[i];
            result.g2pId = fromUtf8(outcome.g2pId);
            result.pronunciation = fromUtf8(outcome.pronunciation);
            result.candidates.clear();
            result.candidates.reserve(static_cast<qsizetype>(outcome.candidates.size()));
            for (const auto &candidate : outcome.candidates) {
                result.candidates.append(fromUtf8(candidate));
            }
        }

        return results;
    }
} // namespace FillLyric
