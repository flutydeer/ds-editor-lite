#include "TaskManager.h"
#include "TaskManager_p.h"

#include <QtConcurrent/QtConcurrent>

#include "Task.h"

void BackgroundWorker::terminateTask(Task *task) {
    task->terminate();
}

namespace {
    // A task that never honours termination must not keep the editor's exit
    // waiting forever, so the pool wait is bounded and reports the leftovers.
    int shutdownTimeoutMs() {
        bool ok = false;
        const auto value = qEnvironmentVariableIntValue("DS_EDITOR_SHUTDOWN_TIMEOUT_MS", &ok);
        return ok && value > 0 ? static_cast<int>(value) : 30000;
    }
}

void BackgroundWorker::wait() {
    const auto threadPool = QThreadPool::globalInstance();
    const auto timeout = shutdownTimeoutMs();
    if (!threadPool->waitForDone(timeout)) {
        qWarning() << "Background tasks did not finish within" << timeout
                   << "ms; continuing shutdown with" << threadPool->activeThreadCount()
                   << "task(s) still running";
    }
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

TaskManager *TaskManager::instance() {
    static TaskManager *p = new TaskManager;
    return p;
}

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
    if (!task) {
        qWarning() << "Can not remove null task";
        return;
    }

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
