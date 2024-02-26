//
// Created by fluty on 24-2-26.
//

#ifndef TASKMANAGER_H
#define TASKMANAGER_H

#include <QObject>

#include "../../gui/Utils/Singleton.h"

class ITask;

class TaskManager : public QObject, public Singleton<TaskManager> {
    Q_OBJECT
public:
    const QList<ITask *> &tasks() const;
    void addTask(ITask *task);
    void startTask(ITask *task);
    // void startTask(int taskId);
    void startAllTasks();
    void terminateTask(ITask *task);
    void terminateAllTasks();

private:
    QList<ITask *>m_tasks;
};



#endif // TASKMANAGER_H
