#include "TaskManager.h"
#include "TaskManager_p.h"

#include <QtConcurrent/QtConcurrent>

#include "Task.h"

void BackgroundWorker::terminateTask(Task *task) {
    task->terminate();
}

void BackgroundWorker::wait() {
    // A task that never honours termination keeps its runtime, file guard and model
    // pointers alive, and the teardown destroys those right after this call. A wait
    // that gave up would therefore be a use-after-free, and leaking the pool only
    // moves it to whenever the task resumes. The join is unbounded on purpose: this
    // library owns cancellation, waiting and ownership only, and a process that
    // cannot finish is ended by the application-level, process-lifetime watchdog
    // instead of by an exit policy hidden in a generic task pool.
    QThreadPool::globalInstance()->waitForDone();
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
