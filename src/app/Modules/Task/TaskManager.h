//
// Created by fluty on 24-2-26.
//

#ifndef TASKMANAGER_H
#define TASKMANAGER_H

#define taskManager TaskManager::instance()

#include <QThreadPool>

#include "Utils/Singleton.h"

class Task;
class TaskManagerPrivate;

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
    static void terminateTask(Task *task);
    void terminateAllTasks();
    void wait();

private slots:
    void onWorkerWaitDone();

private:
    Q_DECLARE_PRIVATE(TaskManager)
    TaskManagerPrivate *d_ptr;
};



#endif // TASKMANAGER_H
