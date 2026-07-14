//
// Created by OrangeCat on 24-9-3.
//

#include "GetPhonemeNameTask.h"
#include "PhonemeDistribution.h"

#include "Global/AppGlobal.h"
#include "Global/SingingClipSlicerGlobal.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/SynthrtEngine/SynthrtEngine.h"

#include <QDebug>
#include <QLoggingCategory>
#include <QHash>
#include <QSet>

Q_LOGGING_CATEGORY(logInferPhoneme, "infer.phoneme_name")

GetPhonemeNameTask::GetPhonemeNameTask(const int clipId, const quint64 clipRevision,
                                       const QList<NoteInferenceSnapshot> &notes,
                                       const SingerInfo &singerInfo, const double tempo)
    : m_clipSingerInfo(singerInfo), m_clipId(clipId), m_clipRevision(clipRevision), m_inputs(notes),
      m_tempo(tempo) {
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
    newStatus.message = "正在处理: " + m_previewText;
    setStatus(newStatus);
    result = getPhonemeNames();
    distributePhonemes();
}

QList<PhonemeNameResult> GetPhonemeNameTask::getPhonemeNames() {
    if (appStatus->languageModuleStatus != AppStatus::ModuleStatus::Ready) {
        // R3/TD-3: 改 qFatal 为 qCCritical + 返回等长 fallback 结果，与 GetPronunciationTask
        // 同场景（cpp:71-74）对齐。原 qFatal 直接 abort 进程，虽有 InferController
        // 守卫但设计不一致。 返回等长 list（每个 PhonemeNameResult 默认 success=false）保证
        // distributePhonemes 的 result[i] 访问不越界。
        qCCritical(logInferPhoneme) << "Language module not ready yet, using fallback";
        m_success.store(false, std::memory_order_release);
        return QList<PhonemeNameResult>(m_inputs.size());
    }
    // R14/TD-21: fallback singer（Pending/Missing）时不调 LanguageService，返回等长 fallback
    // 与 resolveLanguageRoute 的 valid 语义对齐：非 Resolved 视为不可路由
    if (m_clipSingerInfo.resolutionState() != ResolutionState::Resolved) {
        qCWarning(logInferPhoneme) << "SingerInfo not resolved, skip phoneme fetch. identifier:"
                                   << m_clipSingerInfo.identifier();
        m_success.store(false, std::memory_order_release);
        return QList<PhonemeNameResult>(m_inputs.size());
    }

    // S2P conversion via SynthrtEngine::resolveS2pResource().
    // Successful resources and deterministic failures are cached per language.
    QHash<QString, std::shared_ptr<srt::s2p::LanguageResource>> s2pCache;
    QSet<QString> failedS2pLanguages;

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

            // Resolve S2P resource (cached per language)
            auto it = s2pCache.find(input.language);
            if (it == s2pCache.end()) {
                auto resourceExp = SynthrtEngine::instance().resolveS2pResource(
                    m_clipSingerInfo.identifier(), input.language);
                if (!resourceExp) {
                    failedS2pLanguages.insert(input.language);
                    qCWarning(logInferPhoneme)
                        << "S2P resource resolution failed for language:" << input.language << ":"
                        << QString::fromUtf8(resourceExp.error().message());
                    result.success = false;
                    allSuccess = false;
                    results.append(result);
                    continue;
                }
                it = s2pCache.insert(input.language, *resourceExp);
            }
            const auto &resource = it.value();

            try {
                auto syllable = resource->convert(input.pronunciation.toStdString());
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
            } catch (const std::exception &e) {
                qCWarning(logInferPhoneme)
                    << "S2P conversion failed for pronunciation:" << input.pronunciation << ":"
                    << e.what();
                result.success = false;
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
    const int gapThresholdTicks = static_cast<int>(
        std::round(gapThresholdMs * AppGlobal::ticksPerQuarterNote * m_tempo / 60000.0));
    distributePhonemesToNotes(m_inputs, result, gapThresholdTicks);
}
