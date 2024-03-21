//
// Created by fluty on 24-2-26.
//

#ifndef ITASK_H
#define ITASK_H

#include <QObject>
#include <QRunnable>

#include "Global/TaskGlobal.h"
#include "Utils/UniqueObject.h"

class TaskStatus {
public:
    int id = -1;
    int progress = 0;
    int maximum = 100;
    int minimum = 0;
    bool isIndetermine = false;
    TaskGlobal::Status runningStatus = TaskGlobal::Normal;
    QString title = "Task";
    QString message = "Message";
};

class ITask : public QObject, public QRunnable, public UniqueObject {
    Q_OBJECT
public:
    explicit ITask(QObject *parent = nullptr) : QObject(parent) {
        setAutoDelete(false);
    }
    explicit ITask(int id, QObject *parent = nullptr) : QObject(parent), UniqueObject(id) {
        setAutoDelete(false);
    }
    ~ITask() override {
        terminate();
    };
    void run() override {
        if (!m_started) {
            m_started = true;
            runTask();
        }
    }
    void terminate() {
        m_abortFlag = true;
    }
    [[nodiscard]] bool started() const {
        return m_started;
    }

    [[nodiscard]] const TaskStatus &status() const;
    void setStatus(const TaskStatus &status);

signals:
    void statusUpdated(const TaskStatus &status);
    void finished(bool terminate);

protected:
    virtual void runTask() = 0;
    bool m_abortFlag = false;
    bool m_started = false;

private:
    TaskStatus m_status;
};
inline const TaskStatus &ITask::status() const {
    return m_status;
}
inline void ITask::setStatus(const TaskStatus &status) {
    m_status = status;
    emit statusUpdated(status);
}

#endif // ITASK_H
