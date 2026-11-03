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
    // Preserve original lyrics when the language module is unavailable or conversion fails.
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

    const auto reservedPhonemes = ReservedPhonemes::of(m_singerInfo.identifier());
    auto isSkippedNote = [&reservedPhonemes](const NoteInferenceSnapshot &note) {
        const auto lyric = note.lyric.trimmed();
        if (reservedPhonemes.contains(lyric) || Note::isSlurLyric(lyric))
            return true;
        return lyric.isEmpty() || Note::isSyllabificationLyric(lyric);
    };

    // Batch notes by language in first-seen order. Preserve lyrics on conversion failure.
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

        auto converted = SynthrtEngine::instance().convert(
            identifier, language, words, lite::synthrt::LanguageBridge::Depth::Pronunciation);
        if (!converted) {
            qCWarning(logInferPron).nospace()
                << "G2P conversion failed for lang='" << language
                << "': " << fromUtf8(converted.error().toString()) << ". Keeping original lyric.";
            for (const auto &entry : entries)
                pronResult[entry.first].candidates = {fromUtf8(entry.second)};
            continue;
        }

        const auto outcomes = converted.take();
        // Preserve original lyrics if the result count does not match the request.
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

            // A per-word error must not replace the original lyric with empty pronunciation data.
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

            // Report fallback readings because they may not match the intended pronunciation.
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
