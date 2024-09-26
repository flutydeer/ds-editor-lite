//
// Created by fluty on 24-9-10.
//

#include "GetPronunciationTask.h"

#include "Controller/Utils/NoteWordUtils.h"

#include <QDebug>

GetPronunciationTask::GetPronunciationTask(int clipId, const QList<Note *> &notes)
    : m_clipId(clipId), m_notes(notes) {
    notesRef = notes;
    for (int i = 0; i < notes.count(); i++) {
        m_previewText.append(notes.at(i)->lyric());
        if (i == 20) {
            m_previewText.append("...");
            break;
        }
    }
    TaskStatus status;
    status.title = "获取发音信息";
    status.message = m_previewText;
    status.isIndetermine = true;
    setStatus(status);
}

int GetPronunciationTask::clipId() const {
    return m_clipId;
}

void GetPronunciationTask::runTask(){
    qDebug() << "运行获取发音任务"<<"clipId:" << clipId() << "taskId:" << id();
    result = NoteWordUtils::getPronunciations(m_notes);
    if (isTerminateRequested()) {
        qWarning() << "任务被终止 taskId:" << id();
        return;
    }
    qDebug() << "任务正常完成 taskId:" << id();
}