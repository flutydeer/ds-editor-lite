//
// Created by OrangeCat on 24-9-3.
//

#include "GetPhonemeNameTask.h"
#include "PhonemeDistribution.h"

#include "Global/AppGlobal.h"
#include <lite/ProjectModel/SingingClipSlicer/SingingClipSlicerGlobal.h>
#include "Model/AppStatus/AppStatus.h"
#include <lite/SynthrtEngine/SynthrtEngine.h>

#include <lite/Support/VersionUtils.h>

#include <QDebug>
#include <QLoggingCategory>
#include <QHash>
#include <QSet>

Q_LOGGING_CATEGORY(logInferPhoneme, "infer.phoneme_name")

GetPhonemeNameTask::GetPhonemeNameTask(const int clipId, const quint64 clipRevision,
                                       const QList<NoteInferenceSnapshot> &notes,
                                       const SingerInfo &singerInfo, const Timeline &timeline)
    : m_clipSingerInfo(singerInfo), m_clipId(clipId), m_clipRevision(clipRevision), m_inputs(notes),
      m_timeline(timeline) {
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
    distributePhonemes();
}

QList<PhonemeNameResult> GetPhonemeNameTask::getPhonemeNames() {
    if (appStatus->languageModuleStatus != AppStatus::ModuleStatus::Ready) {
        // R3/TD-3: Replace qFatal with qCCritical + return an equal-length
        // fallback result, aligned with GetPronunciationTask's same scenario
        // (cpp:71-74). The original qFatal aborted the process directly; while
        // InferController guards against it, the design was inconsistent.
        // Returning an equal-length list (each PhonemeNameResult defaults to
        // success=false) ensures distributePhonemes' result[i] access does
        // not go out of bounds.
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

    // B1b-3/B1c: S2P conversion via VoicebankSession::convertS2p().
    // Language module readiness is ensured per language via ensureLanguageReady()
    // (cached internally by the session); deterministic failures are cached in
    // failedS2pLanguages so subsequent inputs in the same language skip fast.
    // Replaces the legacy resolveS2pResource() + LanguageResource::convert() pair
    // removed in B1c.
    QSet<QString> failedS2pLanguages;
    QSet<QString> readyLanguages;

    auto &session = SynthrtEngine::instance().session();
    const auto identifier = m_clipSingerInfo.identifier();
    const auto packageId = identifier.packageId.toStdString();
    const auto version = VersionUtils::qt_to_stdc(identifier.packageVersion);

    QList<PhonemeNameResult> results;
    results.reserve(m_inputs.size());
    bool allSuccess = true;

    for (const auto &input : m_inputs) {
        PhonemeNameResult result;
        if (input.pronunciation == "SP" || input.pronunciation == "AP") {
            PhonemeName restPhoneme;
            restPhoneme.name = input.pronunciation;
            restPhoneme.language = input.language;
            result.phonemeNames.append(restPhoneme);
            result.success = true;
        } else if (input.pronunciation == "-" || input.pronunciation.isEmpty()) {
            result.success = true;
        } else {
            if (failedS2pLanguages.contains(input.language)) {
                allSuccess = false;
                results.append(result);
                continue;
            }

            // Ensure the S2P language module is loaded (cached per language).
            if (!readyLanguages.contains(input.language)) {
                const auto lang = input.language.toStdString();
                auto readyExp = session.ensureLanguageReady(packageId, version, lang);
                if (!readyExp) {
                    failedS2pLanguages.insert(input.language);
                    qCWarning(logInferPhoneme)
                        << "S2P language ready failed for language:" << input.language << ":"
                        << QString::fromUtf8(readyExp.error().message());
                    result.success = false;
                    allSuccess = false;
                    results.append(result);
                    continue;
                }
                readyLanguages.insert(input.language);
            }

            // Convert pronunciation to phonemes. SingerIdentifier implicitly
            // converts to SingerRef (B1a), supplying version-aware routing.
            auto sylExp = session.convertS2p(identifier, input.language.toStdString(),
                                             input.pronunciation.toStdString());
            if (!sylExp) {
                qCWarning(logInferPhoneme)
                    << "S2P conversion failed for pronunciation:" << input.pronunciation << ":"
                    << QString::fromUtf8(sylExp.error().message());
                result.success = false;
                allSuccess = false;
                results.append(result);
                continue;
            }

            const auto &syllable = *sylExp;
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

void GetPhonemeNameTask::distributePhonemes() {
    const double gapThresholdMs = 2.0 * SingingClipSlicerGlobal::padBaseLength;
    const int gapThresholdTicks =
        static_cast<int>(std::round(m_timeline.msToTick(gapThresholdMs)));
    distributePhonemesToNotes(m_inputs, result, gapThresholdTicks);
}
