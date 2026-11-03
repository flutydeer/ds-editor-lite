#include "GetPhonemeNameTask.h"

#include "Modules/Inference/Utils/ReservedPhonemes.h"
#include "Syllabification.h"

#include "Global/AppGlobal.h"
#include "Model/AppStatus/AppStatus.h"
#include <lite/SynthrtEngine/LanguageBridge.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>

#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/Support/VersionUtils.h>

#include <QDebug>
#include <QLoggingCategory>
#include <QHash>
#include <QSet>

#include <optional>

Q_LOGGING_CATEGORY(logInferPhoneme, "infer.phoneme_name")

GetPhonemeNameTask::GetPhonemeNameTask(Automation::DocumentVersion documentVersion,
                                       const int clipId, const quint64 clipRevision,
                                       const QList<NoteInferenceSnapshot> &notes,
                                       const SingerInfo &singerInfo)
    : m_clipSingerInfo(singerInfo), m_clipId(clipId), m_documentVersion(std::move(documentVersion)),
      m_clipRevision(clipRevision), m_inputs(notes) {
    for (int i = 0; i < notes.count(); i++) {
        const auto &note = notes.at(i);
        m_previewText.append(note.lyric);
        if (i == 20) {
            m_previewText.append("...");
            break;
        }
    }
    TaskStatus status;
    status.title = tr("Fetch Phoneme Name");
    status.message = m_previewText;
    status.isIndetermine = true;
    setStatus(status);
    qInfo() << "Task created"
            << " clipId:" << m_clipId << "taskId:" << id() << "taskRevision:" << m_clipRevision
            << "noteCount:" << m_inputs.count();
}

Automation::DocumentVersion GetPhonemeNameTask::documentVersion() const {
    return m_documentVersion;
}

int GetPhonemeNameTask::clipId() const {
    return m_clipId;
}

quint64 GetPhonemeNameTask::clipRevision() const {
    return m_clipRevision;
}

QList<int> GetPhonemeNameTask::noteIds() const {
    QList<int> ids;
    ids.reserve(m_inputs.size());
    for (const auto &note : m_inputs)
        ids.append(note.noteId);
    return ids;
}

bool GetPhonemeNameTask::success() const {
    return m_success.load(std::memory_order_acquire);
}

void GetPhonemeNameTask::runTask() {
    qDebug() << "Running task..."
             << "clipId:" << clipId() << "taskId:" << id();
    processNotes();
    qInfo() << "TaskFinished"
            << "clipId:" << clipId() << "taskId:" << id() << "terminate:" << terminated();
}

void GetPhonemeNameTask::processNotes() {
    auto newStatus = status();
    newStatus.message = tr("Processing: %1").arg(m_previewText);
    setStatus(newStatus);
    result = getPhonemeNames();
    Syllabification::keepPhonemesOnWordRoots(m_inputs, result);
}

QList<PhonemeNameResult> GetPhonemeNameTask::getPhonemeNames() {
    if (appStatus->languageModuleStatus != AppStatus::ModuleStatus::Ready) {
        // Keep the fallback result index-aligned with the input notes.
        qCCritical(logInferPhoneme) << "Language module not ready yet, using fallback";
        m_success.store(false, std::memory_order_release);
        QList<PhonemeNameResult> results(m_inputs.size());
        for (auto &result : results)
            result.errorMessage = tr("Language module is not ready");
        return results;
    }
    if (m_clipSingerInfo.resolutionState() != ResolutionState::Resolved) {
        qCWarning(logInferPhoneme) << "SingerInfo not resolved, skip phoneme fetch. identifier:"
                                   << m_clipSingerInfo.identifier();
        m_success.store(false, std::memory_order_release);
        QList<PhonemeNameResult> results(m_inputs.size());
        for (auto &result : results)
            result.errorMessage = tr("Singer is not available");
        return results;
    }

    // Cache route failures per language for the rest of this batch.
    QHash<QString, QString> failedLanguages;

    // Cache conversion depth per language. An unusable route still requests conversion to report
    // its error.
    QHash<QString, std::optional<lite::synthrt::LanguageBridge::Depth>> languageDepths;

    const auto identifier = m_clipSingerInfo.identifier();
    const auto reservedPhonemes = ReservedPhonemes::of(identifier);

    QList<PhonemeNameResult> results;
    results.reserve(m_inputs.size());
    bool allSuccess = true;

    for (const auto &input : m_inputs) {
        PhonemeNameResult result;
        if (reservedPhonemes.contains(input.pronunciation)) {
            PhonemeName restPhoneme;
            restPhoneme.name = input.pronunciation;
            restPhoneme.language = input.language;
            restPhoneme.isOnset = true;
            result.phonemeNames.append(restPhoneme);
            result.success = true;
        } else if (Note::isSlurLyric(input.lyric) ||
                   Syllabification::isSyllabificationLyric(input.lyric) ||
                   input.pronunciation == "-" || input.pronunciation.isEmpty()) {
            result.success = true;
        } else {
            if (failedLanguages.contains(input.language)) {
                allSuccess = false;
                result.errorMessage = failedLanguages.value(input.language);
                results.append(result);
                continue;
            }

            // Limit the request to the deepest layer the language provides.
            if (!languageDepths.contains(input.language)) {
                languageDepths.insert(input.language,
                                      SynthrtEngine::instance().maxDepth(identifier, input.language));
            }
            const auto limit = languageDepths.value(input.language);
            if (limit == lite::synthrt::LanguageBridge::Depth::Pronunciation) {
                // Pronunciation-only routes have no phoneme result to fetch.
                result.success = true;
                results.append(result);
                continue;
            }

            // Pin pronunciation to skip G2P; request phonemes with onsets when the route supports
            // them.
            std::vector<lite::synthrt::LanguageBridge::Word> words;
            words.push_back({input.lyric.toStdString(), input.pronunciation.toStdString(), {}, {}});
            // An unusable route is not a shallow valid route; let conversion report the failure.
            auto converted = SynthrtEngine::instance().convert(
                identifier, input.language, words,
                limit.value_or(lite::synthrt::LanguageBridge::Depth::Onsets));
            if (!converted) {
                failedLanguages.insert(
                    input.language, tr("Failed to load the phoneme module for language %1")
                                        .arg(input.language));
                qCWarning(logInferPhoneme)
                    << "S2P conversion failed for language:" << input.language << ":"
                    << QString::fromStdString(converted.error().toString());
                result.success = false;
                result.errorMessage = failedLanguages.value(input.language);
                allSuccess = false;
                results.append(result);
                continue;
            }

            const auto outcomes = converted.take();
            if (outcomes.empty() || !outcomes.front().error.empty()) {
                qCWarning(logInferPhoneme)
                    << "S2P conversion failed for pronunciation:" << input.pronunciation << ":"
                    << (outcomes.empty()
                            ? QStringLiteral("no result")
                            : QString::fromStdString(outcomes.front().error));
                result.success = false;
                result.errorMessage =
                    tr("Failed to convert the pronunciation of \"%1\"").arg(input.pronunciation);
                allSuccess = false;
                results.append(result);
                continue;
            }

            const auto &syllable = outcomes.front();
            // Missing onsets are valid for phoneme-only routes; nonempty onsets must align with
            // phonemes.
            if (!syllable.onsets.empty() && syllable.onsets.size() != syllable.phonemes.size()) {
                qCWarning(logInferPhoneme)
                    << "the composition reported" << static_cast<quint64>(syllable.onsets.size())
                    << "onsets for" << static_cast<quint64>(syllable.phonemes.size())
                    << "phonemes; the result is rejected. language:" << input.language;
                result.success = false;
                result.errorMessage = tr("The onset markers do not match the phonemes for \"%1\"")
                                          .arg(input.pronunciation);
                allSuccess = false;
                results.append(result);
                continue;
            }
            for (size_t k = 0; k < syllable.phonemes.size(); ++k) {
                PhonemeName pn;
                pn.name = QString::fromStdString(syllable.phonemes[k]);
                pn.language = input.language;
                pn.isOnset = (k < syllable.onsets.size()) ? syllable.onsets[k] : false;
                result.phonemeNames.append(pn);
            }
            result.success = !result.phonemeNames.isEmpty();
            if (!result.success) {
                // No per-note detail here: the slicer-level diagnostics already
                // explain the missing phonemes, and the pronunciation is visible
                // on the note itself
                qCWarning(logInferPhoneme)
                    << "S2P returned empty phonemes for pronunciation:" << input.pronunciation;
                allSuccess = false;
            }
        }
        results.append(result);
    }

    m_success.store(allSuccess, std::memory_order_release);
    return results;
}
