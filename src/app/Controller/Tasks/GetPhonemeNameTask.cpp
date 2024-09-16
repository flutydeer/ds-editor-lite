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

GetPhonemeNameTask::GetPhonemeNameTask(int clipId, const QList<PhonemeNameInput> &inputs) : clipId(clipId), m_inputs(inputs) {
    for (int i = 0; i < inputs.count(); i++) {
        const auto& note = inputs.at(i);
        m_previewText.append(note.lyric);
        if (i == 20) {
            m_previewText.append("...");
            break;
        }
    }
    TaskStatus status;
    status.title = "获取音素信息";
    status.message = m_previewText;
    status.isIndetermine = true;
    setStatus(status);
}

void GetPhonemeNameTask::runTask() {
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