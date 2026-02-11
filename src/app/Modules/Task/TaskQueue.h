//
// Created by fluty on 24-9-27.
//

#ifndef TASKQUEUE_H
#define TASKQUEUE_H

#include "TaskManager.h"
#include "Task.h"
#include "Utils/Linq.h"
#include "Utils/Queue.h"

#include <QDebug>

template <typename T>
class TaskQueue {
public:
    Queue<T *> pending;
    T *current = nullptr;

    void add(T *task);
    void cancelAll();
    void cancelIf(std::function<bool(T *task)> pred);
    void disposePendingTasks();
    void onCurrentFinished();

private:
    void runNext();
    void disposePendingTask(T *task);
};

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
        // qWarning() << "Task queue is empty";
        return;
    }
    const auto task = pending.dequeue();
    taskManager->startTask(task);
    current = task;
}

template <typename T>
void TaskQueue<T>::cancelAll() {
    for (const auto task : pending) {
        disposePendingTask(task);
    }
    if (current) {
        T *taskToCancel = current;

        // Disconnect all existing finished handlers to prevent them from firing
        QObject::disconnect(taskToCancel, &Task::finished, nullptr, nullptr);

        // Connect cleanup lambda
        QObject::connect(
            taskToCancel, &Task::finished, taskToCancel,
            [taskToCancel]() {
                taskManager->removeTask(taskToCancel);
                taskToCancel->deleteLater();
            },
            Qt::QueuedConnection);

        taskManager->terminateTask(taskToCancel);
        qDebug() << "Terminate current task: "
                 << "taskId:" << taskToCancel->id();
        current = nullptr;
    }
}

template <typename T>
void TaskQueue<T>::cancelIf(std::function<bool(T *task)> pred) {
    for (const auto task : Linq::where(pending, pred)) {
        disposePendingTask(task);
    }
    if (current && pred(current)) {
        // Save current task to local variable for lambda capture
        T *taskToCancel = current;

        // Disconnect all existing finished handlers to prevent them from firing
        // after the task is externally cancelled
        QObject::disconnect(taskToCancel, &Task::finished, nullptr, nullptr);

        // Connect task finished signal for safe cleanup
        QObject::connect(
            taskToCancel, &Task::finished, taskToCancel,
            [taskToCancel]() {
                qDebug() << "Cancelled task finished, safe cleanup: taskId:" << taskToCancel->id();
                taskManager->removeTask(taskToCancel);
                taskToCancel->deleteLater();
            },
            Qt::QueuedConnection);

        taskManager->terminateTask(taskToCancel);
        qDebug() << "Terminate current task and wait for cleanup: taskId:" << taskToCancel->id();

        current = nullptr;
        runNext();
    }
}

template <typename T>
void TaskQueue<T>::disposePendingTasks() {
    for (const auto task : pending)
        disposePendingTask(task);
}

template <typename T>
void TaskQueue<T>::onCurrentFinished() {
    if (!current) {
        qWarning() << "TaskQueue::onCurrentFinished called with null current";
        return;
    }
    current->disconnect();
    taskManager->removeTask(current);
    current = nullptr;

    // Automatically run the next task in the queue
    runNext();
}

template <typename T>
void TaskQueue<T>::disposePendingTask(T *task) {
    qDebug() << "Dispose pending task: "
             << "taskId:" << task->id();
    taskManager->removeTask(task);
    task->disconnect();
    pending.remove(task);
}

#endif // TASKQUEUE_H
