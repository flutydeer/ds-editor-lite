//
// Created by fluty on 24-2-26.
//

#include "TaskManager.h"

#include "ITask.h"

const QList<ITask *> &TaskManager::tasks() const {
    return m_tasks;
}
void TaskManager::addTask(ITask *task) {
    m_tasks.append(task);
}
void TaskManager::startTask(ITask *task) {
    task->execute();
}
void TaskManager::startAllTasks() {
    for (const auto &task : m_tasks)
        task->execute();
}
void TaskManager::terminateTask(ITask *task) {
    task->terminate();
}
void TaskManager::terminateAllTasks() {
    for (const auto &task : m_tasks)
        task->terminate();
}