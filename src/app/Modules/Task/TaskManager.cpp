//
// Created by fluty on 24-2-26.
//

#include "TaskManager.h"

#include <QtConcurrent/QtConcurrent>

#include "Task.h"

void BackgroundWorker::terminateTask(Task *task) {
    task->terminate();
}
void BackgroundWorker::wait() {
    auto threadPool = QThreadPool::globalInstance();
    threadPool->waitForDone();
    emit waitDone();
}
TaskManager::TaskManager(QObject *parent) : QObject(parent) {
    connect(&m_worker, &BackgroundWorker::waitDone, this, &TaskManager::onWorkerWaitDone);
    m_worker.moveToThread(&m_thread);
}
TaskManager::~TaskManager() {
    terminateAllTasks();
}
const QList<Task *> &TaskManager::tasks() const {
    return m_tasks;
}
Task *TaskManager::findTaskById(int id) {
    for (const auto task : m_tasks)
        if (task->id() == id)
            return task;

    return nullptr;
}
void TaskManager::wait() {
    m_worker.wait();
    // threadPool->waitForDone();
}
void TaskManager::addTask(Task *task) {
    qDebug() << "TaskManager::addTask" << task->id();
    auto index = m_tasks.count();
    m_tasks.append(task);
    emit taskChanged(Added, task, index);
}
void TaskManager::startTask(Task *task) {
    qDebug() << "TaskManager::startTask";
    threadPool->start(task);
}
void TaskManager::removeTask(Task *task) {
    auto index = m_tasks.indexOf(task);
    m_tasks.removeOne(task);
    emit taskChanged(Removed, task, index);
}
void TaskManager::startAllTasks() {
    for (const auto &task : m_tasks)
        threadPool->start(task);
}
void TaskManager::terminateTask(Task *task) {
    BackgroundWorker::terminateTask(task);
}
void TaskManager::terminateAllTasks() {
    for (const auto &task : m_tasks)
        BackgroundWorker::terminateTask(task);
}
void TaskManager::onWorkerWaitDone() {
    emit allDone();
}