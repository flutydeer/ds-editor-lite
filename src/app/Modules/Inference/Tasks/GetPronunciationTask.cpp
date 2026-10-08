#include "GetPronunciationTask.h"

#include "Modules/Inference/Utils/PronunciationText.h"
#include "Modules/Inference/Utils/ReservedPhonemes.h"

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
    using PronunciationText::fromUtf8;
    using PronunciationText::toUtf8;
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

    // A lyric that is a reserved phoneme of the singer is its own pronunciation.
    const auto reservedPhonemes = ReservedPhonemes::of(m_singerInfo.identifier());
    auto isSkippedNote = [&reservedPhonemes](const NoteInferenceSnapshot &note) {
        const auto lyric = note.lyric.trimmed();
        if (reservedPhonemes.contains(lyric) || Note::isSlurLyric(lyric))
            return true;
        return lyric.isEmpty() || Note::isSyllabificationLyric(lyric);
    };

    // Group non-skipped notes by language (preserving first-seen order). Each language is
    // converted in one call to the language layer, which wolf routes by singer and language
    // without a G2P identifier. On a routing or conversion failure, the notes of that language
    // keep the original lyric (ds-session.md §206).
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

        // No preparation step is required, because wolf loads a language on its first
        // conversion.
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
        // The caller validates the result count. On a mismatch, the original lyric is kept. No
        // Q_ASSERT is used, because a Debug-build abort conflicts with D11 precise error
        // reporting.
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

            // A word that fails to convert keeps its position in the batch and carries its own
            // error, so an unknown word does not affect the other words of the batch. A failed
            // outcome carries no usable pronunciation or candidates, so the prefilled original
            // lyric is kept instead of being overwritten, as the fill-lyric path also does
            // (FillLyric/Utils/G2pService.cpp).
            if (!outcomes[i].error.empty()) {
                qCWarning(logInferPron).nospace()
                    << "G2P conversion error note[" << noteIdx << "] lyric='"
                    << qPrintable(fromUtf8(entries[i].second)) << "' reason='"
                    << qPrintable(fromUtf8(outcomes[i].error)) << "'";
                continue;
            }

            res.pronunciation = fromUtf8(outcomes[i].pronunciation);
            res.stage = outcomes[i].stage;
            QStringList rawCandidates;
            rawCandidates.reserve(static_cast<qsizetype>(outcomes[i].candidates.size()));
            for (const auto &candidate : outcomes[i].candidates)
                rawCandidates.append(fromUtf8(candidate));
            res.candidates = PronunciationText::normalizeCandidates(res.pronunciation, rawCandidates);

            // A fallback is the last resort of the language module, so it is reported: the reading
            // may not match how the word is pronounced, and no surface of the editor says where a
            // pronunciation came from.
            if (outcomes[i].stage == lite::synthrt::PronunciationStage::Fallback) {
                qCWarning(logInferPron).nospace()
                    << "G2P used the fallback for note[" << noteIdx << "] lyric='"
                    << qPrintable(fromUtf8(entries[i].second)) << "' pronunciation='"
                    << qPrintable(res.pronunciation) << "'";
            }
        }
    }

    return pronResult;
}
