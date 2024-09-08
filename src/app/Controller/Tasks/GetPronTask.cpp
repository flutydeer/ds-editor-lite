//
// Created by OrangeCat on 24-9-3.
//

#include "GetPronTask.h"

#include "Controller/Utils/NoteWordUtils.h"
#include "Model/AppModel/Note.h"
#include "Utils/AppModelUtils.h"

#include <QDebug>
#include <QEventLoop>
#include <QMutexLocker>
#include <QThread>

GetPronTask::GetPronTask(int clipId, const QList<Note *> &notes) : clipId(clipId) {
    notesRef = notes;
    AppModelUtils::copyNotes(notes, m_notes);
    for (int i = 0; i < notes.count(); i++) {
        auto note = notes.at(i);
        m_previewText.append(note->lyric());
        if (i == 20) {
            m_previewText.append("...");
            break;
        }
    }
    TaskStatus status;
    status.title = "获取发音和音素信息";
    status.message = m_previewText;
    status.isIndetermine = true;
    setStatus(status);
}

void GetPronTask::runTask() {
    // if (appStatus->languageModuleStatus == AppStatus::ModuleStatus::Ready)
    processNotes();
    /*else {
        qDebug() << "Waiting for language module ready";
        auto newStatus = status();
        newStatus.message = "正在等待语言模块就绪... ";
        setStatus(newStatus);

        QEventLoop loop;

        // 连接信号，在模块状态就绪时退出事件循环
        connect(appStatus, &AppStatus::moduleStatusChanged, this,
                [&loop, this](AppStatus::ModuleType module, AppStatus::ModuleStatus status) {
                    if (module == AppStatus::ModuleType::Language &&
                        status != AppStatus::ModuleStatus::Loading) {
                        loop.quit();
                    }
                });

        // 运行事件循环，阻塞当前线程，直到信号触发
        loop.exec();

        // 信号触发后，执行耗时操作
        if (appStatus->languageModuleStatus == AppStatus::ModuleStatus::Ready)
            processNotes();
        else {
            qCritical()
                << "Failed to get pronunciation and phoneme: Language module not ready";
            emit finished(false);
        }
    }*/
    if (isTerminateRequested()) {
        qWarning() << "任务被终止 taskId:" << id();
        emit finished();
        return;
    }
    qDebug() << "任务正常完成 taskId:" << id();
    emit finished();
}

void GetPronTask::processNotes() {
    // qDebug() << "Language module ready, start to process notes";
    auto newStatus = status();
    newStatus.message = "正在处理: " + m_previewText;
    setStatus(newStatus);
    result = NoteWordUtils::getOriginalWordProperties(m_notes);
}