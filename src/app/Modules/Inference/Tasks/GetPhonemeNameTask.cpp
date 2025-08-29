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

    QList<QPair<QString, QString>> inputs;
    for (const auto &note : m_inputs) {
        // 如果发音已编辑，则使用已编辑的发音作为获取音素名称的输入
        inputs.append({note.language, note.pronunciation});
    }
    result = getPhonemeNames(inputs);
}

QList<PhonemeNameResult>
    GetPhonemeNameTask::getPhonemeNames(const QList<QPair<QString, QString>> &input) {
    if (appStatus->languageModuleStatus != AppStatus::ModuleStatus::Ready) {
        qFatal() << "Language module not ready yet";
        return {};
    }
    const auto s2pMgr = S2pMgr::instance();
    QList<PhonemeNameResult> phonemeNameResult;
    phonemeNameResult.reserve(input.size());
    for (const auto [language, pronunciation] : input) {
        // qInfo() << pronunciation;
        PhonemeNameResult note;
        if (pronunciation == "SP" || pronunciation == "AP") {
            note.normalNames.append(pronunciation);
        } else {
            if (const auto phonemes = s2pMgr->syllableToPhoneme(
                    m_clipSingerId, m_clipSingerInfo.g2pId(language), pronunciation);
                !phonemes.empty()) {
                if (phonemes.size() == 1) {
                    note.normalNames.append(phonemes.at(0));
                } else if (phonemes.size() == 2) {
                    note.aheadNames.append(phonemes.at(0));
                    note.normalNames.append(phonemes.at(1));
                } else
                    qCritical() << "Cannot handle more than 2 phonemes" << phonemes;
            } else if (pronunciation != "-") {
                qCritical() << "Failed to get phoneme names of pronunciation: " << "language:"
                            << language << "g2pId:" << m_clipSingerInfo.g2pId(language)
                            << "pronunciation:" << pronunciation;
                return {};
            }
        }
        phonemeNameResult.append(note);
    }

    m_success.store(true, std::memory_order_release);
    return phonemeNameResult;
}