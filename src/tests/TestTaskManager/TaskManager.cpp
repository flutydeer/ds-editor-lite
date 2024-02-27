//
// Created by fluty on 24-2-26.
//

#include "TaskManager.h"

#include "ITask.h"

void BackgroundWorker::terminateTask(ITask *task) {
    task->terminate();
}
TaskManager::TaskManager(QObject *parent) : QObject(parent) {
    m_worker.moveToThread(&m_thread);
}
TaskManager::~TaskManager() {
    terminateAllTasks();
}
const QList<ITask *> &TaskManager::tasks() const {
    return m_tasks;
}
void TaskManager::addTask(ITask *task) {
    m_tasks.append(task);
}
void TaskManager::startTask(ITask *task) {
    threadPool->start(task);
}
void TaskManager::startAllTasks() {
    for (const auto &task : m_tasks)
        threadPool->start(task);
}
void TaskManager::terminateTask(ITask *task) {
    m_worker.terminateTask(task);
}
void TaskManager::terminateAllTasks() {
    for (const auto &task : m_tasks)
        m_worker.terminateTask(task);
}