//
// Created by fluty on 24-2-26.
//

#include "TestTask.h"
TestTask::TestTask(const QString &inputData, QObject *parent) : ITask(parent) {
    m_inputData = inputData;
}
QString TestTask::resultData() {
    m_resultData = QString("result: ") + m_inputData;
    return m_resultData;
}
void TestTask::runTask() {
    emit progressUpdated(0);
    for (int i = 1; i <= 20; i++) {
        if (m_thread.isInterruptionRequested()) {
            QThread::sleep(2);
            emit finished(true);
            return;
        }
        QThread::msleep(500);
        emit progressUpdated(i * 5);
    }
    emit finished(false);
    m_thread.requestInterruption();
}