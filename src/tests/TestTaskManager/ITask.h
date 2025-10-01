//
// Created by fluty on 24-2-26.
//

#ifndef ITASK_H
#define ITASK_H

#include <QObject>
#include <QRunnable>

class ITask : public QObject, public QRunnable {
    Q_OBJECT
public:
    enum RunningStatus { Normal, Warning, Error };
    explicit ITask(QObject *parent = nullptr) : QObject(parent) {
    }
    ~ITask() override {
        terminate();
    };
    void run() override {
        runTask();
    }
    void terminate() {
        m_abortFlag = true;
    }

signals:
    void statusUpdated(int progress, ITask::RunningStatus status, bool isIndeterminate);
    void finished(bool terminate);

protected:
    virtual void runTask() = 0;
    bool m_abortFlag = false;
};

#endif // ITASK_H
