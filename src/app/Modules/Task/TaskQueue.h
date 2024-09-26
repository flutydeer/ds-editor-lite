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

    void runNext();
    void cancelIf(std::function<bool(T *task)> pred);
};

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
void TaskQueue<T>::cancelIf(std::function<bool(T *task)> pred) {
    for (const auto task : Linq::where(pending, pred)) {
        task->disconnect();
        pending.remove(task);
        taskManager->removeTask(task);
        qDebug() << "Remove pending task"
                 << "taskId:" << task->id();
        delete task;
    }
    if (current && pred(current)) {
        taskManager->terminateTask(current);
        qDebug() << "Terminate current task"
                 << "taskId:" << current->id();
    }
}

#endif // TASKQUEUE_H
