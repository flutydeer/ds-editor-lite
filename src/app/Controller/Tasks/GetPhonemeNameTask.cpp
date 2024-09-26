//
// Created by OrangeCat on 24-9-3.
//

#include "GetPhonemeNameTask.h"

#include "Controller/Utils/NoteWordUtils.h"
#include "Model/AppModel/Note.h"
#include "Utils/AppModelUtils.h"

#include <QDebug>
// #include <QEventLoop>
#include <QMutexLocker>
#include <QThread>

GetPhonemeNameTask::GetPhonemeNameTask(int clipId, const QList<PhonemeNameInput> &inputs)
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
    status.title = "获取音素名称";
    status.message = m_previewText;
    status.isIndetermine = true;
    setStatus(status);
}

int GetPhonemeNameTask::clipId() const {
    return m_clipId;
}

void GetPhonemeNameTask::runTask() {
    qDebug() << "Running task..."
             << "clipId:" << clipId() << "taskId:" << id();
    processNotes();
    if (isTerminateRequested()) {
        qWarning() << "任务被终止 taskId:" << id();
        return;
    }
    qDebug() << "任务正常完成 taskId:" << id();
}

void GetPhonemeNameTask::processNotes() {
    // qDebug() << "Language module ready, start to process notes";
    auto newStatus = status();
    newStatus.message = "正在处理: " + m_previewText;
    setStatus(newStatus);

    QList<QString> inputs;
    for (const auto &note : m_inputs) {
        // 如果发音已编辑，则使用已编辑的发音作为获取音素名称的输入
        inputs.append(note.pronunciation);
    }
    result = NoteWordUtils::getPhonemeNames(inputs);
}