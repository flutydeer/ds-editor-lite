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
    // R14/TD-21: For fallback singers (Pending/Missing) LanguageService is
    // not invoked; return an equal-length fallback aligned with
    // resolveLanguageRoute's valid semantics: non-Resolved is non-routable.
    if (m_clipSingerInfo.resolutionState() != ResolutionState::Resolved) {
        qCWarning(logInferPhoneme) << "SingerInfo not resolved, skip phoneme fetch. identifier:"
                                   << m_clipSingerInfo.identifier();
        m_success.store(false, std::memory_order_release);
        QList<PhonemeNameResult> results(m_inputs.size());
        for (auto &result : results)
            result.errorMessage = tr("Singer is not available");
        return results;
    }

    // A language that fails once fails identically for every input in this batch. It is
    // therefore recorded, and its remaining inputs are skipped. No other state is cached here,
    // because wolf loads a language on its first conversion and keeps it loaded. The reason
    // travels with the language, so that an input which skips straight past reports the same
    // one as the input that failed.
    QHash<QString, QString> failedLanguages;

    // Deepest layer that each language reaches, as reported by the language module. Cached per
    // language for the same reason as the failures above: the value belongs to the language, and a
    // phrase of five hundred notes asks once. An empty value is a language whose route is not
    // usable; the request is then made as before, so that the conversion still reports the failure.
    QHash<QString, std::optional<lite::synthrt::LanguageBridge::Depth>> languageDepths;

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
                result.errorMessage = failedLanguages.value(input.language);
                results.append(result);
                continue;
            }

            // The layer that this language reaches is read from the language module, which owns the
            // limit: a composition without an Onset member stops at Phonemes, and one without an S2P
            // member stops at Pronunciation. A deeper request silently returns the shallower result,
            // so the depth below is the smaller of what this task displays and that limit. The read
            // happens once per language, because the limit belongs to the language and not to the
            // note.
            if (!languageDepths.contains(input.language)) {
                languageDepths.insert(input.language,
                                      SynthrtEngine::instance().maxDepth(identifier, input.language));
            }
            const auto limit = languageDepths.value(input.language);
            if (limit == lite::synthrt::LanguageBridge::Depth::Pronunciation) {
                // The composition produces no phoneme at all. That is the shape of the language and
                // not a failure, so neither a conversion nor an error is produced here: the
                // pronunciation that GetPronunciationTask fetched remains the result of the note.
                result.success = true;
                results.append(result);
                continue;
            }

            // The pronunciation is fixed, so the conversion skips grapheme-to-phoneme conversion
            // and only splits this syllable into phonemes, which is the purpose of this task. The
            // editor displays the phoneme that begins a syllable, so the deepest layer this task
            // needs is Onsets; a language that stops at Phonemes is requested at Phonemes instead,
            // and its result then carries no onsets, which is a legal result. This relies on the
            // composition producing phonemes whenever its limit is deeper than Pronunciation, which
            // the language module guarantees by binding an S2P member before it reports that limit.
            std::vector<lite::synthrt::LanguageBridge::Word> words;
            words.push_back({input.lyric.toStdString(), input.pronunciation.toStdString(), {}, {}});
            // A route that is not usable reports no layer at all. The requested depth is then the
            // one this task always asked for, so that the conversion itself decides success and
            // failure: an unusable route is not equally a language without an onset layer.
            auto converted = SynthrtEngine::instance().convert(
                identifier, input.language, words,
                limit.value_or(lite::synthrt::LanguageBridge::Depth::Onsets));
            if (!converted) {
                // A missing route is also missing for the remaining inputs of this language.
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
            // The depth requested above is the deepest layer this language reaches, so a result
            // without onsets is the legal result of a language that stops at the phoneme layer: no
            // phoneme of such a syllable begins it. Asking for that shallower depth is new here,
            // because before this task read the limit it always requested Onsets, and a composition
            // without an onset member then marked every phoneme as a non-onset. Only a result that
            // carries onsets and yet does not align them one to one with the phonemes contradicts
            // the contract, and that is reported because the loop below would otherwise record a
            // partially wrong onset list.
            if (!syllable.onsets.empty() && syllable.onsets.size() != syllable.phonemes.size()) {
                qCWarning(logInferPhoneme)
                    << "the composition reported" << static_cast<quint64>(syllable.onsets.size())
                    << "onsets for" << static_cast<quint64>(syllable.phonemes.size())
                    << "phonemes; the rest are treated as non-onsets. language:" << input.language;
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
