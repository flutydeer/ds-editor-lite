//
// Created by fluty on 24-2-26.
//

#ifndef TASKMANAGER_H
#define TASKMANAGER_H

#include <QObject>
#include <QThreadPool>

#include "../../app/Utils/Singleton.h"

class ITask;

class BackgroundWorker : public QObject {
    Q_OBJECT
public:
    static void terminateTask(ITask *task);
    void wait();

signals:
    void waitDone();
};

class TaskManager : public QObject, public Singleton<TaskManager> {
    Q_OBJECT
public:
    explicit TaskManager(QObject *parent = nullptr);
    ~TaskManager() override;
    [[nodiscard]] const QList<ITask *> &tasks() const;

signals:
    void allDone();

public slots:
    void addTask(ITask *task);
    void startTask(ITask *task);
    // void startTask(int taskId);
    void startAllTasks();
    static void terminateTask(ITask *task);
    void terminateAllTasks();
    void wait();

private slots:
    void onWorkerWaitDone();

private:
    QList<ITask *> m_tasks;
    QThreadPool *threadPool = QThreadPool::globalInstance();
    BackgroundWorker m_worker;
    QThread m_thread;
};



#endif // TASKMANAGER_H
