//
// Created by fluty on 24-2-26.
//

#ifndef TASK_H
#define TASK_H

#include <atomic>
#include <cstdint>
#include <type_traits>

#include <QObject>
#include <QRunnable>
#include <QReadWriteLock>

#include "Global/TaskGlobal.h"
#include "Utils/UniqueObject.h"

class TaskStatus {
public:
    int id = -1;
    int progress = 0;
    int maximum = 100;
    int minimum = 0;
    bool isIndetermine = false;
    TaskGlobal::Status runningStatus = TaskGlobal::Normal;
    QString title = "Task";
    QString message = "Message";
};

class Task : public QObject, public QRunnable, public UniqueObject {
    Q_OBJECT
public:
    enum Flags : uint8_t {
        TaskStarted = 1 << 0,
        TaskAbortRequested = 1 << 1,
        TaskStopped = 1 << 2,
    };
    using FlagType = std::underlying_type_t<Flags>;

    explicit Task(QObject *parent = nullptr) : QObject(parent) {
        setAutoDelete(false);
    }

    explicit Task(int id, QObject *parent = nullptr) : QObject(parent), UniqueObject(id) {
        setAutoDelete(false);
    }

    // ~Task() override {
    //     terminate();
    // };

    virtual void terminate() {
        setFlag(TaskAbortRequested);
    }

    [[nodiscard]] bool started() const {
        return checkFlag(TaskStarted);
    }

    bool stopped() const {
        return checkFlag(TaskStopped);
    }

    bool terminated() const {
        return checkFlag(TaskAbortRequested);
    }

    int priority() const {
        return m_priority.load(std::memory_order_acquire);
    }

    void setPriority(const int priority) {
        m_priority.store(priority, std::memory_order_release);
    }

    [[nodiscard]] const TaskStatus &status() const {
        return m_status;
    }

    void setStatus(const TaskStatus &status) {
        m_status = status;
        emit statusUpdated(status);
    }

signals:
    void statusUpdated(const TaskStatus &status);
    void finished();

protected:
    virtual void runTask() = 0;

    bool isTerminateRequested() const {
        return checkFlag(TaskAbortRequested);
    }

    std::atomic<int> m_priority{0};

private:
    friend class TaskManager;

    bool checkFlag(const FlagType flag) const {
        return m_taskFlags.load(std::memory_order_acquire) & flag;
    }

    void setFlag(const FlagType flag) {
        m_taskFlags.fetch_or(flag, std::memory_order_release);
    }

    void unsetFlag(const FlagType flag) {
        m_taskFlags.fetch_and(~flag, std::memory_order_release);
    }

    void run() override {
        // CAS
        FlagType expected = m_taskFlags.load(std::memory_order_acquire);
        FlagType desired;

        do {
            if (expected & TaskStarted) {
                return;
            }
            desired = expected | TaskStarted;
        } while (!m_taskFlags.compare_exchange_weak(expected, desired, std::memory_order_acq_rel,
                                                    std::memory_order_acquire));

        runTask();
        emit finished();

        setFlag(TaskStopped);
    }

    TaskStatus m_status;
    mutable QReadWriteLock m_statusLock;
    std::atomic<FlagType> m_taskFlags{0};
};

#endif // TASK_H
