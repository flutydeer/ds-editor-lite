#include "GetPhonemeNameTask.h"

#include "Modules/Inference/Utils/ReservedPhonemes.h"
#include "Syllabification.h"

#include "Global/AppGlobal.h"
#include "Model/AppStatus/AppStatus.h"
#include <lite/SynthrtEngine/SynthrtEngine.h>

#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/Support/VersionUtils.h>

#include <QDebug>
#include <QLoggingCategory>
#include <QHash>
#include <QSet>

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
        return QList<PhonemeNameResult>(m_inputs.size());
    }
    // R14/TD-21: For fallback singers (Pending/Missing) LanguageService is
    // not invoked; return an equal-length fallback aligned with
    // resolveLanguageRoute's valid semantics: non-Resolved is non-routable.
    if (m_clipSingerInfo.resolutionState() != ResolutionState::Resolved) {
        qCWarning(logInferPhoneme) << "SingerInfo not resolved, skip phoneme fetch. identifier:"
                                   << m_clipSingerInfo.identifier();
        m_success.store(false, std::memory_order_release);
        return QList<PhonemeNameResult>(m_inputs.size());
    }

    // A language that fails once fails identically for every input in this batch. It is
    // therefore recorded, and its remaining inputs are skipped. No other state is cached here,
    // because wolf loads a language on its first conversion and keeps it loaded.
    QSet<QString> failedLanguages;

    const auto identifier = m_clipSingerInfo.identifier();
    // A pronunciation that is a reserved phoneme is sung as that phoneme and is not converted.
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
                results.append(result);
                continue;
            }

            // The pronunciation is fixed, so the conversion skips grapheme-to-phoneme conversion
            // and only splits this syllable into phonemes, which is the purpose of this task. The
            // requested depth is Onsets rather than Phonemes because the editor displays the
            // phoneme that begins a syllable; a separate request would convert twice.
            std::vector<lite::synthrt::LanguageBridge::Word> words;
            words.push_back({input.lyric.toStdString(), input.pronunciation.toStdString(), {}, {}});
            auto converted = SynthrtEngine::instance().convert(
                identifier, input.language, words, lite::synthrt::LanguageBridge::Depth::Onsets);
            if (!converted) {
                // A missing route is also missing for the remaining inputs of this language.
                failedLanguages.insert(input.language);
                qCWarning(logInferPhoneme)
                    << "S2P conversion failed for language:" << input.language << ":"
                    << QString::fromStdString(converted.error().toString());
                result.success = false;
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
                allSuccess = false;
                results.append(result);
                continue;
            }

            const auto &syllable = outcomes.front();
            for (size_t k = 0; k < syllable.phonemes.size(); ++k) {
                PhonemeName pn;
                pn.name = QString::fromStdString(syllable.phonemes[k]);
                pn.language = input.language;
                pn.isOnset = (k < syllable.onsets.size()) ? syllable.onsets[k] : false;
                result.phonemeNames.append(pn);
            }
            result.success = !result.phonemeNames.isEmpty();
            if (!result.success) {
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
