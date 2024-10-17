//
// Created by fluty on 24-9-27.
//

#ifndef TASKQUEUE_H
#define TASKQUEUE_H

#include "TaskManager.h"
#include "Utils/Linq.h"
#include "Utils/Queue.h"

// #include <type_traits>

template <typename T>
class TaskQueue {
    // static_assert(std::is_base_of_v<Task, T>, "T must be a derived class of Task");

public:
    Queue<T *> pending;
    T *current = nullptr;

    void add(T *task);
    void runNext();
    void cancelAll();
    void cancelIf(std::function<bool(T *task)> pred);
    void disposePendingTasks();
    void onCurrentFinished();

private:
    void disposePendingTask(T *task);
};

template <typename T>
void TaskQueue<T>::add(T *task) {
    taskManager->addTask(task);
    pending.enqueue(task);
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
        taskManager->terminateTask(current);
        qDebug() << "Terminate current task: "
                 << "taskId:" << current->id();
    }
}

template <typename T>
void TaskQueue<T>::disposePendingTasks() {
    for (const auto task : pending)
        disposePendingTask(task);
}

template <typename T>
void TaskQueue<T>::onCurrentFinished() {
    current->disconnect();
    taskManager->removeTask(current);
    current = nullptr;
}

template <typename T>
void TaskQueue<T>::disposePendingTask(T *task) {
    qDebug() << "Dispose pending task: "
             << "taskId:" << task->id();
    taskManager->removeTask(task);
    task->disconnect();
    pending.remove(task);
    delete task;
}

#endif // TASKQUEUE_H
