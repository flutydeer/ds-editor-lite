//
// Created by fluty on 24-2-26.
//

#ifndef ITASK_H
#define ITASK_H

#include <QObject>
#include <QThread>

class ITask : public QObject {
    Q_OBJECT
public:
    explicit ITask(QObject *parent = nullptr) : QObject(parent) {
    }
    ~ITask() override = default;
    void execute() {
        moveToThread(&m_thread);
        connect(&m_thread, &QThread::started, this, &ITask::runTask);
        connect(this, &ITask::finished, &m_thread, &QThread::quit);
        // connect(&m_thread, &QThread::finished, &m_thread, &QThread::deleteLater);
        m_thread.start();
    }
    void terminate() {
        m_thread.requestInterruption();
    }

signals:
    void progressUpdated(int progress);
    void finished(bool terminate);

protected:
    virtual void runTask() = 0;
    bool m_abortFlag = false;
    QThread m_thread;
};

#endif // ITASK_H
