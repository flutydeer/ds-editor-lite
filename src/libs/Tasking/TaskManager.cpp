#include "TaskManager.h"
#include "TaskManager_p.h"

#include <QtConcurrent/QtConcurrent>

#include "Task.h"

#include <QMutexLocker>
#include <QPointer>

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

QList<Task *> TaskManager::tasksForDocument(const QUuid &documentId) const {
    Q_D(const TaskManager);
    QList<Task *> result;
    for (auto *task : d->m_tasks) {
        if (task->documentId() == documentId)
            result.append(task);
    }
    return result;
}

void TaskManager::setDocumentIdProvider(std::function<QUuid()> provider) {
    Q_D(TaskManager);
    d->documentIdProvider = std::move(provider);
}

void TaskManager::wait() {
    Q_D(TaskManager);
    d->m_worker.wait();
}

void TaskManager::addTask(Task *task) {
    Q_D(TaskManager);
    if (task->documentId().isNull() && d->documentIdProvider)
        task->setDocumentId(d->documentIdProvider());
    connect(
        task, &Task::finished, this, [d] { d->completionCondition.wakeAll(); },
        Qt::DirectConnection);
    connect(
        task, &QObject::destroyed, this, [d] { d->completionCondition.wakeAll(); },
        Qt::DirectConnection);
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

void TaskManager::terminateTasks(const QUuid &documentId) {
    for (auto *task : tasksForDocument(documentId))
        BackgroundWorker::terminateTask(task);
}

void TaskManager::waitForDocument(const QUuid &documentId) {
    Q_D(TaskManager);
    QList<QPointer<Task>> pending;
    for (auto *task : tasksForDocument(documentId))
        pending.append(task);

    QMutexLocker locker(&d->completionMutex);
    while (std::any_of(pending.cbegin(), pending.cend(), [](const QPointer<Task> &task) {
        return task && task->started() && !task->stopped();
    })) {
        d->completionCondition.wait(&d->completionMutex, 50);
    }
}

void TaskManager::onWorkerWaitDone() {
    qDebug() << "TaskManager allDone";
    emit allDone();
}
