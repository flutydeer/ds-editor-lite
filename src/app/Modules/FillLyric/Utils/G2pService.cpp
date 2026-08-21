#include "Modules/FillLyric/Utils/G2pService.h"

#include <QLoggingCategory>

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include <synthrt/G2P/LanguageService.h>

#include "Modules/FillLyric/Utils/TextTagger.h"
#include <lite/Language/G2pConvertRunner.h>
#include <lite/Language/G2pInputAdapter.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>
#include <lite/Support/VersionUtils.h>
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

        /// Candidates from the g2p engine may be the split phoneme tokens of the
        /// pronunciation itself (dict step) rather than true alternative
        /// pronunciations; collapse them to the whole pronunciation so the
        /// UI never offers single phonemes as switchable candidates.
        QStringList normalizePronunciationCandidates(const QString &pronunciation,
                                                     QStringList candidates) {
            if (pronunciation.isEmpty())
                return candidates;
            const auto pronTokens = pronunciation.split(u' ', Qt::SkipEmptyParts);
            if (pronTokens.isEmpty())
                return candidates;
            for (auto &c : candidates)
                c = c.trimmed();
            candidates.removeAll(QString());
            const bool allArePronTokens =
                !candidates.isEmpty() &&
                std::all_of(candidates.cbegin(), candidates.cend(), [&](const QString &c) {
                    return c.contains(u' ') ? c == pronunciation : pronTokens.contains(c);
                });
            if (allArePronTokens)
                return {pronunciation};
            return candidates;
        }
    }

    G2pService::G2pService(SingerIdentifier singer,
                           const srt::g2p::LanguageService &languageService)
        : m_singer(std::move(singer)) {
        Q_UNUSED(languageService);
    }

    QList<G2pResult> G2pService::convert(const QList<LangNote> &notes,
                                         const std::vector<std::string> &priorityLanguages) const {

        // Pre-fill results: all notes default to original lyric preservation
        // (pronunciation=lyric, candidates={lyric}) per ds-session.md §206.
        // This guarantees the returned list is the same length as the input so
        // callers do not go out of bounds. G2P fallback is forbidden by
        // ds-session.md §196; on failure the host only preserves the lyric.
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
                << "Singer identifier is empty; keeping original lyric for all notes";
            return results;
        }

        // Group by resolved language (preserving first-seen order):
        // language -> [(noteIndex, lyricUtf8), ...].
        // lyric has its trailing '+' stripped, matching the old implementation
        // (which chopped the trailing '+' before fromRoute).
        std::map<QString, std::vector<std::pair<int, std::string>>> langGroups;
        const int commonCount = qMin(static_cast<int>(taggerRes.size()), notes.size());
        for (int i = 0; i < commonCount; i++) {
            const auto language = notes[i].language == QStringLiteral("unknown")
                                      ? fromUtf8(taggerRes[i].language)
                                      : notes[i].language;

            auto lyric = notes[i].lyric;
            while (lyric.endsWith('+'))
                lyric.chop(1);

            langGroups[language].emplace_back(i, toUtf8(lyric));
        }

        if (langGroups.empty())
            return results;

        // B1b-3: Each language calls session().convertG2p once (internally
        // routed by SingerRef.version, filling g2pId/g2pContext/
        // g2pContextVersion). On route or conversion failure an Expected
        // error is returned and all notes in that language keep the original
        // lyric (ds-session.md §206: G2P failure preserves lyric).
        auto &session = SynthrtEngine::instance().session();
        const auto packageId = m_singer.packageId.toStdString();
        const auto version = VersionUtils::qt_to_stdc(m_singer.packageVersion);
        for (const auto &[language, entries] : langGroups) {
            std::vector<srt::g2p::G2pInput> inputs;
            inputs.reserve(entries.size());
            for (const auto &entry : entries) {
                srt::g2p::G2pInput input;
                input.lyric = entry.second;
                inputs.push_back(std::move(input));
            }

            // Ensure the G2P language module is loaded before conversion.
            // convertG2p does not auto-initialize models; ensureLanguageReady
            // loads them lazily on first call (cached internally by the session).
            const auto langStd = toUtf8(language);
            auto readyExp = session.ensureLanguageReady(packageId, version, langStd);
            if (!readyExp) {
                qCWarning(logFillG2p) << "G2P language ready failed for language" << language << ":"
                                      << fromUtf8(readyExp.error().message());
                continue; // Keep original lyric for this language
            }

            auto exp = session.convertG2p(m_singer, langStd, inputs);
            if (!exp) {
                qCWarning(logFillG2p) << "Failed to convert G2P for language" << language << ":"
                                      << fromUtf8(exp.error().message());
                continue; // Keep original lyric for this language
            }

            const auto &outcomes = *exp;
            if (outcomes.size() != entries.size()) {
                qCWarning(logFillG2p)
                    << "convertG2p returned" << outcomes.size() << "outcomes for" << entries.size()
                    << "requests; keeping original lyric for unmatched notes";
            }

            const auto coveredCount = std::min(outcomes.size(), entries.size());
            for (size_t i = 0; i < coveredCount; i++) {
                const auto noteIdx = entries[i].first;
                auto &result = results[noteIdx];

                result.language = notes[noteIdx].language == QStringLiteral("unknown")
                                      ? fromUtf8(taggerRes[noteIdx].language)
                                      : notes[noteIdx].language;

                const auto &outcome = outcomes[i];
                result.g2pId = fromUtf8(outcome.g2pId);
                result.pronunciation = fromUtf8(outcome.pronunciation);
                QStringList rawCandidates;
                rawCandidates.reserve(static_cast<qsizetype>(outcome.candidates.size()));
                for (const auto &candidate : outcome.candidates) {
                    rawCandidates.append(fromUtf8(candidate));
                }
                result.candidates =
                    normalizePronunciationCandidates(result.pronunciation, rawCandidates);
            }
        }

        return results;
    }
} // namespace FillLyric
