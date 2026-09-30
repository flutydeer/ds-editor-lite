#include "Modules/FillLyric/Utils/G2pService.h"

#include <QLoggingCategory>

#include <algorithm>
#include <map>
#include <utility>
#include <vector>


#include "Modules/FillLyric/Utils/TextTagger.h"
#include <lite/SynthrtEngine/SynthrtEngine.h>
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

    G2pService::G2pService(SingerIdentifier singer) : m_singer(std::move(singer)) {
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

        // One call per language. Which linguist answers it is decided by the singer's own
        // language map, so nothing here has to know a route; a language that cannot be converted
        // leaves its notes with the original lyric (ds-session.md §206).
        for (const auto &[language, entries] : langGroups) {
            std::vector<lite::synthrt::LanguageBridge::Word> words;
            words.reserve(entries.size());
            for (const auto &entry : entries) {
                words.push_back({entry.second, {}, {}, {}});
            }

            auto converted = SynthrtEngine::instance().convert(
                m_singer, language, words, lite::synthrt::LanguageBridge::Depth::Pronunciation);
            if (!converted) {
                qCWarning(logFillG2p) << "Failed to convert for language" << language << ":"
                                      << fromUtf8(converted.error().toString());
                continue; // Keep the original lyric for this language
            }

            const auto outcomes = converted.take();
            if (outcomes.size() != entries.size()) {
                qCWarning(logFillG2p)
                    << "the conversion returned" << outcomes.size() << "outcomes for"
                    << entries.size() << "requests; keeping original lyric for unmatched notes";
            }

            const auto coveredCount = std::min(outcomes.size(), entries.size());
            for (size_t i = 0; i < coveredCount; i++) {
                const auto noteIdx = entries[i].first;
                auto &result = results[noteIdx];

                result.language = notes[noteIdx].language == QStringLiteral("unknown")
                                      ? fromUtf8(taggerRes[noteIdx].language)
                                      : notes[noteIdx].language;

                const auto &outcome = outcomes[i];
                if (!outcome.error.empty()) {
                    continue; // This word keeps its original lyric; the rest of the batch stands.
                }
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
