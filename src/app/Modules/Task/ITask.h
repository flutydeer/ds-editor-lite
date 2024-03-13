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
    enum TaskStatus { Normal, Warning, Error };
    explicit ITask(QObject *parent = nullptr) : QObject(parent) {
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

signals:
    void statusUpdated(int progress, ITask::TaskStatus status, bool isIndeterminate);
    void finished(bool terminate);

protected:
    virtual void runTask() = 0;
    bool m_abortFlag = false;
    bool m_started = false;
};

#endif // ITASK_H
