//
// Created by OrangeCat on 24-9-3.
//

#include "GetPhonemeNameTask.h"

#include "Model/AppStatus/AppStatus.h"
#include "Modules/Language/S2pMgr.h"

#include <QDebug>
#include <QMutexLocker>
#include <QThread>

GetPhonemeNameTask::GetPhonemeNameTask(const SingingClip &clip,
                                       const QList<PhonemeNameInput> &inputs)
    : m_clipSingerInfo(clip.singerInfo()), m_clipId(clip.id()), m_inputs(inputs) {
    const auto singerInfo = clip.singerInfo();
    m_clipSingerId = singerInfo.name();

    for (int i = 0; i < inputs.count(); i++) {
        const auto &note = inputs.at(i);
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
            << " clipId:" << m_clipId << "taskId:" << id() << "noteCount:" << m_inputs.count();
}

int GetPhonemeNameTask::clipId() const {
    return m_clipId;
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
    // qDebug() << "Language module ready, start to process notes";
    auto newStatus = status();
    newStatus.message = "正在处理: " + m_previewText;
    setStatus(newStatus);
    result = getPhonemeNames();
}

QList<PhonemeNameResult> GetPhonemeNameTask::getPhonemeNames() {
    if (appStatus->languageModuleStatus != AppStatus::ModuleStatus::Ready) {
        qCritical() << "Language module not ready yet";
        return {};
    }
    const auto s2pMgr = S2pMgr::instance();
    QList<PhonemeNameResult> results;
    results.reserve(m_inputs.size());
    bool allSuccess = true;

    for (const auto &input : m_inputs) {
        PhonemeNameResult result;
        if (input.pronunciation == "SP" || input.pronunciation == "AP") {
            PhonemeName restPhoneme;
            restPhoneme.name = input.pronunciation;
            result.phonemeNames.append(restPhoneme);
            result.success = true;
        } else {
            if (const auto phonemes = s2pMgr->syllableToPhoneme(
                    m_clipSingerInfo.identifier(), m_clipSingerInfo.g2pId(input.language),
                    input.pronunciation);
                !phonemes.empty()) {
                if (phonemes.size() == 1) {
                    PhonemeName phoneme;
                    phoneme.name = phonemes.at(0);
                    phoneme.language = input.language;
                    phoneme.isOnset = true;
                    result.phonemeNames.append(phoneme);
                    result.success = true;
                } else if (phonemes.size() == 2) {
                    // TODO 为英语优化，g2p输出带有卡拍信息的音素
                    PhonemeName firstPhoneme;
                    firstPhoneme.name = phonemes.at(0);
                    firstPhoneme.language = input.language;
                    firstPhoneme.isOnset = false;
                    result.phonemeNames.append(firstPhoneme);

                    PhonemeName secondPhoneme;
                    secondPhoneme.name = phonemes.at(1);
                    secondPhoneme.language = input.language;
                    secondPhoneme.isOnset = true;
                    result.phonemeNames.append(secondPhoneme);
                    result.success = true;
                } else {
                    qCritical() << "Cannot handle more than 2 phonemes" << phonemes;
                }
            } else if (input.pronunciation == "-") {
                result.success = true;
            } else {
                qCritical() << "Failed to get phoneme names of pronunciation: " << "language:"
                            << input.language << "g2pId:" << m_clipSingerInfo.g2pId(input.language)
                            << "pronunciation:" << input.pronunciation;
                allSuccess = false;
            }
        }
        results.append(result);
    }

    m_success.store(allSuccess, std::memory_order_release);
    return results;
}

// Syllabification
std::pair<bool, int> GetPhonemeNameTask::checkTrailingPlus(const QString &lyric) {
    if (!lyric.endsWith('+')) {
        return {false, 0};
    }
    int count = 0;
    for (int i = lyric.length() - 1; i >= 0 && lyric[i] == '+'; --i) {
        ++count;
    }
    return {true, count};
}

// TODO: 应该移动到公共方法？
const QList<GetPhonemeNameTask::Syllable>
    GetPhonemeNameTask::splitSyllables(const QList<PhonemeName> &phonemes) {
    QList<Syllable> syllables;
    Syllable currentSyllable;
    bool hasOnset = false;

    for (const auto &phoneme : phonemes) {
        if (phoneme.isOnset && hasOnset) {
            syllables.append(currentSyllable);
            currentSyllable = Syllable();
            hasOnset = false;
        }
        if (phoneme.isOnset) {
            hasOnset = true;
        }
        currentSyllable.phonemes.append(phoneme);
    }

    if (!currentSyllable.phonemes.isEmpty()) {
        syllables.append(currentSyllable);
    }

    return syllables;
}