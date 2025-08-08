//
// Created by OrangeCat on 24-9-3.
//

#include "GetPhonemeNameTask.h"

#include "Model/AppModel/Note.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Language/S2p.h"

#include <QDebug>
#include <QMutexLocker>
#include <QThread>

GetPhonemeNameTask::GetPhonemeNameTask(const int clipId, const QList<PhonemeNameInput> &inputs)
    : m_clipId(clipId), m_inputs(inputs) {
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
            << " clipId:" << clipId << "taskId:" << id() << "noteCount:" << m_inputs.count();
}

int GetPhonemeNameTask::clipId() const {
    return m_clipId;
}

bool GetPhonemeNameTask::success() const {
    return m_success;
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
        inputs.append({note.pronunciation, note.language});
    }
    result = getPhonemeNames(inputs);
}

QList<PhonemeNameResult>
    GetPhonemeNameTask::getPhonemeNames(const QList<QPair<QString, QString>> &input) {
    if (appStatus->languageModuleStatus != AppStatus::ModuleStatus::Ready) {
        qFatal() << "Language module not ready yet";
        return {};
    }
    const auto syllable2p = S2p::instance();
    QList<PhonemeNameResult> phonemeNameResult;
    phonemeNameResult.reserve(input.size());
    for (const auto &[pronunciation, language] : input) {
        // qInfo() << pronunciation;
        PhonemeNameResult note;
        if (pronunciation == "SP" || pronunciation == "AP") {
            note.normalNames.append(pronunciation);
        } else {
            if (const auto phonemes = syllable2p->syllableToPhoneme(pronunciation, language);
                !phonemes.empty()) {
                if (phonemes.size() == 1) {
                    note.normalNames.append(phonemes.at(0));
                } else if (phonemes.size() == 2) {
                    note.aheadNames.append(phonemes.at(0));
                    note.normalNames.append(phonemes.at(1));
                } else
                    qCritical() << "Cannot handle more than 2 phonemes" << phonemes;
            } else if (pronunciation != "-") {
                qCritical() << "Failed to get phoneme names of pronunciation:" << pronunciation;
                return {};
            }
        }
        phonemeNameResult.append(note);
    }

    m_success = true;
    return phonemeNameResult;
}