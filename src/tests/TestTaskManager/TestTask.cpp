//
// Created by fluty on 24-2-26.
//

#include "TestTask.h"

#include <QThread>

TestTask::TestTask(const QString &inputData, QObject *parent) : ITask(parent) {
    m_inputData = inputData;
}

QString TestTask::resultData() {
    m_resultData = QString("result: ") + m_inputData;
    return m_resultData;
}

void TestTask::runTask() {
    emit statusUpdated(0, Normal, false);
    for (int i = 1; i <= 20; i++) {
        if (m_abortFlag) {
            QThread::sleep(2);
            emit finished(true);
            return;
        }
        QThread::msleep(500);
        emit statusUpdated(i * 5, m_abortFlag ? Error : Normal, m_abortFlag);
    }
    emit finished(false);
}