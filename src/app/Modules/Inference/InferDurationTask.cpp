//
// Created by OrangeCat on 24-9-4.
//

#include "InferDurationTask.h"

#include "Model/AppModel/Note.h"
#include "Utils/AppModelUtils.h"

#include <QThread>

InferDurationTask::InferDurationTask(int clipId, const QList<Note *> &notes) : Task(clipId) {
    AppModelUtils::copyNotes(notes, m_notes);
    for (const auto note : m_notes) {
        for (const auto &phoneme : note->phonemeInfo().original) {
            m_previewText.append(phoneme.name + " ");
        }
    }
    TaskStatus status;
    status.title = "正在推理音素长度...";
    status.message = m_previewText;
    status.maximum = m_notes.count();
    setStatus(status);
}

InferDurationTask::~InferDurationTask() {
    for (const auto note : m_notes)
        delete note;
}

QList<Phoneme> InferDurationTask::result() {
    QMutexLocker locker(&m_mutex);
    return m_result;
}

void InferDurationTask::runTask() {
    qDebug() << "run task";
    int i = 0;
    auto newStatus = status();
    for (const auto note : m_notes) {
        QThread::msleep(5);
        if (isTerminateRequested()) {
            newStatus = status();
            newStatus.message = "正在停止: " + m_previewText;
            newStatus.runningStatus = TaskGlobal::Error;
            setStatus(newStatus);
            QThread::sleep(2);
            emit finished(true);
            return;
        }
        for (const auto &phoneme : note->phonemeInfo().original) {
            auto resultPhoneme = phoneme;
            if (phoneme.type == Phoneme::Ahead) {
                resultPhoneme.start = 40;
            }
            m_result.append(resultPhoneme);
        }
        i++;

        newStatus = status();
        newStatus.progress = i;
        setStatus(newStatus);
    }
    emit finished(false);
}