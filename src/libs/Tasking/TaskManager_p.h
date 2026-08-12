#ifndef TASKMANAGER_P_H
#define TASKMANAGER_P_H

#include <QThreadPool>
#include <QMutex>
#include <QWaitCondition>

#include <functional>

class Task;

class BackgroundWorker : public QObject {
    Q_OBJECT
public:
    static void terminateTask(Task *task);
    void wait();

signals:
    void waitDone();
};

class TaskManager;

class TaskManagerPrivate {
    Q_DECLARE_PUBLIC(TaskManager)

public:
    explicit TaskManagerPrivate(TaskManager *q) : q_ptr(q) {
    }

    QList<Task *> m_tasks;
    QThreadPool *threadPool = QThreadPool::globalInstance();
    BackgroundWorker m_worker;
    QThread m_thread;
    std::function<QUuid()> documentIdProvider;
    QMutex completionMutex;
    QWaitCondition completionCondition;

private:
    TaskManager *q_ptr;
};

#endif // TASKMANAGER_P_H
