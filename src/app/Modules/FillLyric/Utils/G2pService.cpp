#include "Modules/FillLyric/Utils/G2pService.h"

#include <QLoggingCategory>

#include <algorithm>
#include <map>
#include <utility>
#include <vector>


#include "Modules/FillLyric/Utils/TextTagger.h"
#include "Modules/Inference/Utils/PronunciationText.h"
#include "Modules/Inference/Utils/ReservedPhonemes.h"
#include <lite/Language/EditorMarkers.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>
Q_LOGGING_CATEGORY(logFillG2p, "fill.g2p")

namespace FillLyric {
    namespace {
        using PronunciationText::fromUtf8;
        using PronunciationText::toUtf8;
    }

    G2pService::G2pService(SingerIdentifier singer) : m_singer(std::move(singer)) {
    }

    QList<G2pResult> G2pService::convert(const QList<LangNote> &notes,
                                         const std::vector<std::string> &priorityLanguages) const {

        // Preserve result indices and original lyrics when conversion fails.
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

        // Group by language and strip continuation markers before conversion.
        std::map<QString, std::vector<std::pair<int, std::string>>> langGroups;
        const auto reserved = ReservedPhonemes::of(m_singer);
        const int commonCount = qMin(static_cast<int>(taggerRes.size()), notes.size());
        for (int i = 0; i < commonCount; i++) {
            if (reserved.contains(notes[i].lyric))
                continue;

            if (Language::isEditorMarker(notes[i].lyric))
                continue;
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

        // Convert once per language; failed conversions preserve the original lyrics.
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
                    qCWarning(logFillG2p) << "Failed to convert lyric" << notes[noteIdx].lyric
                                         << "for language" << language << ":"
                                         << fromUtf8(outcome.error);
                    continue;
                }
                result.pronunciation = fromUtf8(outcome.pronunciation);
                QStringList rawCandidates;
                rawCandidates.reserve(static_cast<qsizetype>(outcome.candidates.size()));
                for (const auto &candidate : outcome.candidates) {
                    rawCandidates.append(fromUtf8(candidate));
                }
                result.candidates =
                    PronunciationText::normalizeCandidates(result.pronunciation, rawCandidates);
            }
        }

        return results;
    }
} // namespace FillLyric
