//
// Created by fluty on 24-2-26.
//

#include "TaskManager.h"
#include "TaskManager_p.h"

#include <QtConcurrent/QtConcurrent>

#include "Task.h"

void BackgroundWorker::terminateTask(Task *task) {
    task->terminate();
}

void BackgroundWorker::wait() {
    const auto threadPool = QThreadPool::globalInstance();
    threadPool->waitForDone();
    emit waitDone();
}

TaskManager::TaskManager(QObject *parent) : QObject(parent), d_ptr(new TaskManagerPrivate(this)) {
    Q_D(TaskManager);
    connect(&d->m_worker, &BackgroundWorker::waitDone, this, &TaskManager::onWorkerWaitDone);
    d->m_worker.moveToThread(&d->m_thread);
}

TaskManager::~TaskManager() {
    terminateAllTasks();
}

LITE_SINGLETON_IMPLEMENT_INSTANCE(TaskManager)

const QList<Task *> &TaskManager::tasks() const {
    Q_D(const TaskManager);
    return d->m_tasks;
}

Task *TaskManager::findTaskById(const int id) {
    Q_D(const TaskManager);
    for (const auto task : d->m_tasks)
        if (task->id() == id)
            return task;

    return nullptr;
}

void TaskManager::wait() {
    Q_D(TaskManager);
    d->m_worker.wait();
    // threadPool->waitForDone();
}

void TaskManager::addTask(Task *task) {
    Q_D(TaskManager);
    qDebug() << "addTask:" << task->id() << task->status().title;
    const auto index = d->m_tasks.count();
    d->m_tasks.append(task);
    emit taskChanged(Added, task, index);
}

void TaskManager::startTask(Task *task) {
    Q_D(TaskManager);
    qDebug() << "startTask" << task->id() << task->status().title;
    d->threadPool->start(task);
}

void TaskManager::addAndStartTask(Task *task) {
    addTask(task);
    startTask(task);
}

void TaskManager::removeTask(Task *task) {
    Q_D(TaskManager);
    const auto index = d->m_tasks.indexOf(task);
    if (index >= 0) {
        d->m_tasks.removeOne(task);
        emit taskChanged(Removed, task, index);
    } else
        qWarning() << "Can not remove task: " << task->objectName();
}

void TaskManager::startAllTasks() {
    Q_D(TaskManager);
    for (const auto &task : d->m_tasks)
        startTask(task);
}

void TaskManager::terminateTask(Task *task) {
    BackgroundWorker::terminateTask(task);
}

void TaskManager::terminateAllTasks() {
    Q_D(TaskManager);
    for (const auto &task : d->m_tasks)
        BackgroundWorker::terminateTask(task);
}

void TaskManager::onWorkerWaitDone() {
    qDebug() << "TaskManager allDone";
    emit allDone();
}