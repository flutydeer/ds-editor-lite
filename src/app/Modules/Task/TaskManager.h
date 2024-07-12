//
// Created by fluty on 24-2-26.
//

#ifndef TASKMANAGER_H
#define TASKMANAGER_H

#include <QObject>
#include <QThreadPool>

#include "Utils/Singleton.h"

class Task;

class BackgroundWorker : public QObject {
    Q_OBJECT
public:
    static void terminateTask(Task *task);
    void wait();

signals:
    void waitDone();
};

class TaskManager : public QObject, public Singleton<TaskManager> {
    Q_OBJECT
public:
    enum TaskChangeType { Added, Removed };
    explicit TaskManager(QObject *parent = nullptr);
    ~TaskManager() override;
    [[nodiscard]] const QList<Task *> &tasks() const;
    Task *findTaskById(int id);

signals:
    void allDone();
    void taskChanged(TaskChangeType type, Task *task, qsizetype index);

public slots:
    void addTask(Task *task);
    void startTask(Task *task);
    void removeTask(Task *task);
    // void startTask(int taskId);
    void startAllTasks();
    void terminateTask(Task *task);
    void terminateAllTasks();
    void wait();

private slots:
    void onWorkerWaitDone();

private:
    QList<Task *> m_tasks;
    QThreadPool *threadPool = QThreadPool::globalInstance();
    BackgroundWorker m_worker;
    QThread m_thread;
};



#endif // TASKMANAGER_H
