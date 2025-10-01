//
// Created by fluty on 24-2-26.
//

#include "TaskManager.h"

#include <QtConcurrent/QtConcurrent>

#include "ITask.h"

void BackgroundWorker::terminateTask(ITask *task) {
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

const QList<ITask *> &TaskManager::tasks() const {
    return m_tasks;
}

void TaskManager::wait() {
    m_worker.wait();
    // threadPool->waitForDone();
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
    BackgroundWorker::terminateTask(task);
}

void TaskManager::terminateAllTasks() {
    for (const auto &task : m_tasks)
        BackgroundWorker::terminateTask(task);
}

void TaskManager::onWorkerWaitDone() {
    emit allDone();
}