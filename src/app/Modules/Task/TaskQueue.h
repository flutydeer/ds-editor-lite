//
// Created by fluty on 24-9-27.
//

#ifndef TASKQUEUE_H
#define TASKQUEUE_H

#include "TaskManager.h"
#include "Task.h"
#include <lite/Support/Linq.h>
#include <lite/ADT/Queue.h>

#include <QDebug>
#include <QPointer>

template <typename T>
class TaskQueue {
public:
    Queue<T *> pending;
    T *current = nullptr;

    void setPriorityComparator(std::function<bool(T *, T *)> comparator);
    void add(T *task);
    void cancelAll();
    void cancelIf(std::function<bool(T *task)> pred);
    void disposePendingTasks();
    bool isCurrent(T *task) const;
    bool onCurrentFinished(T *task = nullptr);

private:
    void runNext();
    void disposePendingTask(T *task);
    void cleanupCancelledCurrent(T *task);
    std::function<bool(T *, T *)> m_comparator;
    bool m_currentCancellationPending = false;
};

template <typename T>
void TaskQueue<T>::setPriorityComparator(std::function<bool(T *, T *)> comparator) {
    m_comparator = std::move(comparator);
}

template <typename T>
void TaskQueue<T>::add(T *task) {
    taskManager->addTask(task);
    pending.enqueue(task);
    if (!current)
        runNext();
}

template <typename T>
void TaskQueue<T>::runNext() {
    current = nullptr;
    if (pending.count() <= 0) {
        return;
    }
    if (m_comparator)
        pending.sort(m_comparator);
    const auto task = pending.dequeue();
    current = task;
    m_currentCancellationPending = false;
    taskManager->startTask(task);
}

template <typename T>
void TaskQueue<T>::cancelAll() {
    const auto pendingTasks = pending.toList();
    for (const auto task : pendingTasks) {
        disposePendingTask(task);
    }
    if (current) {
        taskManager->terminateTask(current);
        qDebug() << "Terminate current task: "
                 << "taskId:" << current->id();
    }
}

template <typename T>
void TaskQueue<T>::cancelIf(std::function<bool(T *task)> pred) {
    for (const auto task : Linq::where(pending, pred)) {
        disposePendingTask(task);
    }
    if (current && pred(current)) {
        if (m_currentCancellationPending) {
            qDebug() << "Current task cancellation is already pending: "
                     << "taskId:" << current->id();
            taskManager->terminateTask(current);
            return;
        }

        T *taskToCancel = current;
        m_currentCancellationPending = true;

        if (taskToCancel->stopped()) {
            QPointer<T> taskPtr(taskToCancel);
            QMetaObject::invokeMethod(
                taskToCancel,
                [this, taskPtr] {
                    if (taskPtr)
                        cleanupCancelledCurrent(taskPtr);
                },
                Qt::QueuedConnection);
        } else {
            QPointer<T> taskPtr(taskToCancel);
            // Wait for the task to actually finish before running the next one,
            // to avoid concurrent access to shared inference resources.
            QObject::connect(
                taskToCancel, &Task::finished, taskToCancel,
                [this, taskPtr]() {
                    if (taskPtr)
                        cleanupCancelledCurrent(taskPtr);
                },
                Qt::QueuedConnection);
        }

        taskManager->terminateTask(taskToCancel);
        qDebug() << "Terminate current task and wait for cleanup: taskId:" << taskToCancel->id();
    }
}

template <typename T>
void TaskQueue<T>::disposePendingTasks() {
    const auto pendingTasks = pending.toList();
    for (const auto task : pendingTasks)
        disposePendingTask(task);
}

template <typename T>
bool TaskQueue<T>::isCurrent(T *task) const {
    return current && current == task;
}

template <typename T>
bool TaskQueue<T>::onCurrentFinished(T *task) {
    if (!current) {
        qWarning() << "Ignore finished task because queue has no current task";
        return false;
    }

    if (task && current != task) {
        qDebug() << "Ignore finished task that is no longer current"
                 << "taskId:" << task->id();
        return false;
    }

    if (m_currentCancellationPending) {
        qDebug() << "Finish cancelled current task"
                 << "taskId:" << current->id();
        const auto finishedTask = current;
        finishedTask->disconnect();
        taskManager->removeTask(finishedTask);
        current = nullptr;
        m_currentCancellationPending = false;
        finishedTask->deleteLater();
        runNext();
        return true;
    }

    const auto finishedTask = current;
    finishedTask->disconnect();
    taskManager->removeTask(finishedTask);
    current = nullptr;
    finishedTask->deleteLater();

    // Automatically run the next task in the queue
    runNext();
    return true;
}

template <typename T>
void TaskQueue<T>::cleanupCancelledCurrent(T *task) {
    qDebug() << "Cancelled task finished, safe cleanup: taskId:" << task->id();
    if (current != task)
        return;

    current = nullptr;
    m_currentCancellationPending = false;
    taskManager->removeTask(task);
    task->deleteLater();
    runNext();
}

template <typename T>
void TaskQueue<T>::disposePendingTask(T *task) {
    if (!pending.remove(task))
        return;

    qDebug() << "Dispose pending task: "
             << "taskId:" << task->id();
    taskManager->removeTask(task);
    task->disconnect();
    delete task;
}

#endif // TASKQUEUE_H
