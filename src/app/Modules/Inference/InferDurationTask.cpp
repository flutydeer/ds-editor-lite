//
// Created by OrangeCat on 24-9-4.
//

#include "InferDurationTask.h"

#include <QThread>
#include <QDebug>

InferDurationTask::InferDurationTask(int clipId, int pieceId, const QList<InferDurNote> &input)
    : clipId(clipId), pieceId(pieceId), m_notes(input) {
    buildPreviewText();
    TaskStatus status;
    status.title = "推理音素长度";
    status.message = "正在等待：" + m_previewText;
    status.maximum = m_notes.count();
    setStatus(status);
}

QList<InferDurNote> InferDurationTask::result() {
    QMutexLocker locker(&m_mutex);
    return m_notes;
}

void InferDurationTask::runTask() {
    int i = 0;
    auto newStatus = status();
    newStatus.message = "正在推理: " + m_previewText;
    for (auto &note : m_notes) {
        QThread::msleep(5);
        if (isTerminateRequested()) {
            abort();
            return;
        }
        // 生成测试长度
        for (int aheadIndex = 0; aheadIndex < note.aheadNames.count(); aheadIndex++)
            note.aheadOffsets.append(40 * (note.aheadNames.count() - aheadIndex));
        for (int normalIndex = 0; normalIndex < note.normalNames.count(); normalIndex++) {
            const int phLen = note.length / note.normalNames.count();
            note.normalOffsets.append(normalIndex * phLen);
        }
        i++;

        newStatus.progress = i;
        setStatus(newStatus);
    }
    if (isTerminateRequested()) {
        abort();
        return;
    }
    qDebug() << "任务正常完成 taskId:" << id();
}

void InferDurationTask::abort() {
    qWarning() << "任务被终止 taskId:" << id();
    auto newStatus = status();
    newStatus.message = "正在停止: " + m_previewText;
    newStatus.isIndetermine = true;
    setStatus(newStatus);
    // QThread::sleep(2);
}

void InferDurationTask::buildPreviewText() {
    for (const auto &note : m_notes) {
        for (const auto &phoneme : note.aheadNames)
            m_previewText.append(phoneme + " ");
        for (const auto &phoneme : note.normalNames)
            m_previewText.append(phoneme + " ");
    }
}