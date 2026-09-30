#include "GetPronunciationTask.h"

#include "Model/AppStatus/AppStatus.h"

#include <QDebug>
#include <QLoggingCategory>
#include <QStringList>

#include <map>
#include <optional>
#include <utility>
#include <vector>

#include <lite/SynthrtEngine/LanguageBridge.h>

#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>

Q_LOGGING_CATEGORY(logInferPron, "infer.pronunciation")

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
    /// pronunciations. In that case collapse them to the whole pronunciation
    /// so the UI never offers single phonemes as switchable candidates.
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

GetPronunciationTask::GetPronunciationTask(Automation::DocumentVersion documentVersion,
                                           const int clipId, const quint64 clipRevision,
                                           const QList<NoteInferenceSnapshot> &notes,
                                           const SingerInfo &singerInfo)
    : m_clipId(clipId), m_documentVersion(std::move(documentVersion)), m_clipRevision(clipRevision),
      m_singerInfo(singerInfo), m_notes(notes) {
    for (int i = 0; i < notes.count(); i++) {
        m_previewText.append(notes.at(i).lyric);
        if (i == 20) {
            m_previewText.append("...");
            break;
        }
    }
    TaskStatus status;
    status.title = tr("Fetch Pronunciation");
    status.message = m_previewText;
    status.isIndetermine = true;
    setStatus(status);
    qInfo() << "GetPronunciationTask created"
            << "clipId:" << clipId << "taskId:" << id() << "taskRevision:" << m_clipRevision;
}

Automation::DocumentVersion GetPronunciationTask::documentVersion() const {
    return m_documentVersion;
}

int GetPronunciationTask::clipId() const {
    return m_clipId;
}

quint64 GetPronunciationTask::clipRevision() const {
    return m_clipRevision;
}

QList<int> GetPronunciationTask::noteIds() const {
    QList<int> ids;
    ids.reserve(m_notes.size());
    for (const auto &note : m_notes)
        ids.append(note.noteId);
    return ids;
}

void GetPronunciationTask::runTask() {
    qDebug() << "Running pronunciation task"
             << "clipId:" << clipId() << "taskId:" << id();
    result = getPronunciationResults(m_notes);
    qInfo() << "Pronunciation task finished taskId:" << id() << "terminate:" << terminated();
}

QList<PronunciationFetchResult>
    GetPronunciationTask::getPronunciationResults(const QList<NoteInferenceSnapshot> &notes) const {
    // Pre-fill with original lyric: when the language module is not ready or
    // G2P fails later, the original lyric is kept as the pronunciation
    // (ds-session.md §206: G2P failure preserves lyric; no G2P fallback).
    QList<PronunciationFetchResult> pronResult;
    pronResult.resize(notes.count());
    for (int i = 0; i < notes.count(); i++) {
        pronResult[i].pronunciation = notes.at(i).lyric;
        pronResult[i].candidates = {notes.at(i).lyric};
    }

    if (appStatus->languageModuleStatus != AppStatus::ModuleStatus::Ready) {
        qCCritical(logInferPron) << "Language module not ready yet; keeping original lyric";
        return pronResult;
    }

    // R9: resolutionState pre-check (lite UI state concern).
    // Non-Resolved singers are treated as non-routable; original lyric is
    // kept as the pronunciation (aligned with GetPhonemeNameTask semantics).
    if (m_singerInfo.resolutionState() != ResolutionState::Resolved) {
        qCWarning(logInferPron) << "SingerInfo not resolved, skip pronunciation fetch. identifier:"
                                << m_singerInfo.identifier();
        return pronResult;
    }

    auto isSkippedNote = [](const NoteInferenceSnapshot &note) {
        const auto lyric = note.lyric.trimmed();
        if (lyric == "SP" || lyric == "AP" || Note::isSlurLyric(lyric))
            return true;
        return lyric.isEmpty() || Note::isSyllabificationLyric(lyric);
    };

    // B1b-3: Group non-skipped notes by language (preserving first-seen order).
    // Each language calls session().convertG2p once (internally routed by
    // SingerRef.version, which fills g2pId/g2pContext/g2pContextVersion).
    // The inference chain never falls back to official; on route/convert
    // failure that language keeps the original lyric (ds-session.md §206).
    // language -> [(noteIndex, lyricUtf8), ...]
    std::map<QString, std::vector<std::pair<int, std::string>>> langGroups;

    for (int i = 0; i < notes.count(); i++) {
        const auto &note = notes.at(i);
        if (isSkippedNote(note)) {
            pronResult[i].pronunciation = note.lyric.trimmed();
            pronResult[i].candidates = {note.lyric.trimmed()};
            continue;
        }

        auto lyric = note.lyric;
        while (lyric.endsWith('+'))
            lyric.chop(1);

        langGroups[note.language].emplace_back(i, toUtf8(lyric));
    }

    if (langGroups.empty())
        return pronResult;

    const auto identifier = m_singerInfo.identifier();

    for (const auto &[language, entries] : langGroups) {
        std::vector<lite::synthrt::LanguageBridge::Word> words;
        words.reserve(entries.size());
        for (const auto &entry : entries) {
            words.push_back({entry.second, {}, {}, {}});
        }

        // Nothing to make ready first. The older line loaded a language's models before it could
        // convert with them; wolf loads a language the first time a conversion asks for one.
        auto converted = SynthrtEngine::instance().convert(
            identifier, language, words, lite::synthrt::LanguageBridge::Depth::Pronunciation);
        if (!converted) {
            // The inference chain never silently falls back to official; the original lyric is
            // kept so users can adjust it later in PronunciationView (ds-session.md §206).
            qCWarning(logInferPron).nospace()
                << "G2P conversion failed for lang='" << language
                << "': " << fromUtf8(converted.error().toString()) << ". Keeping original lyric.";
            for (const auto &entry : entries)
                pronResult[entry.first].candidates = {fromUtf8(entry.second)};
            continue;
        }

        const auto outcomes = converted.take();
        // Caller is responsible for result count validation; on mismatch the original lyric is
        // kept (no Q_ASSERT: a Debug-build abort conflicts with D11 precise error reporting).
        if (outcomes.size() != entries.size()) {
            qCWarning(logInferPron).nospace()
                << "the conversion returned " << outcomes.size() << " outcomes for "
                << entries.size() << " requests; keeping original lyric for all convert notes";
            for (const auto &entry : entries) {
                auto &res = pronResult[entry.first];
                res.pronunciation = fromUtf8(entry.second);
                res.candidates = {fromUtf8(entry.second)};
            }
            continue;
        }

        for (size_t i = 0; i < outcomes.size(); i++) {
            const auto noteIdx = entries[i].first;
            auto &res = pronResult[noteIdx];
            res.pronunciation = fromUtf8(outcomes[i].pronunciation);
            QStringList rawCandidates;
            rawCandidates.reserve(static_cast<qsizetype>(outcomes[i].candidates.size()));
            for (const auto &candidate : outcomes[i].candidates)
                rawCandidates.append(fromUtf8(candidate));
            res.candidates = normalizePronunciationCandidates(res.pronunciation, rawCandidates);

            // A word that could not be converted keeps its place in the batch and carries its own
            // reason, so one unknown word never costs the rest of the sheet.
            if (!outcomes[i].error.empty()) {
                qCWarning(logInferPron).nospace()
                    << "G2P conversion error note[" << noteIdx << "] lyric='"
                    << qPrintable(fromUtf8(entries[i].second)) << "' reason='"
                    << qPrintable(fromUtf8(outcomes[i].error)) << "'";
            }
        }
    }

    return pronResult;
}
