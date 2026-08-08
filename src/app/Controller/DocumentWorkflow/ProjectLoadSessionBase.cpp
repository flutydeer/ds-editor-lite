#include "ProjectLoadSessionBase.h"

#include <lite/Tasking/Task.h>
#include <lite/Tasking/TaskManager.h>

#include <utility>

ProjectLoadSessionBase::ProjectLoadSessionBase(QString filePath, quint64 requestId, QObject *parent)
    : IProjectLoadSession(parent), m_filePath(std::move(filePath)), m_requestId(requestId) {
}

ProjectLoadSessionBase::~ProjectLoadSessionBase() {
    detachTask();
    detachReprocessTask();
}

void ProjectLoadSessionBase::start() {
    if (m_started || m_terminal)
        return;
    m_started = true;
    onStart();
}

void ProjectLoadSessionBase::cancel() {
    if (m_terminal)
        return;
    m_terminal = true;
    onCancel();
    detachTask();
    detachReprocessTask();
    emit canceled();
}

PreparedProject ProjectLoadSessionBase::takeResult() {
    return std::move(m_result);
}

quint64 ProjectLoadSessionBase::requestId() const {
    return m_requestId;
}

void ProjectLoadSessionBase::startParseTask() {
    if (m_terminal || m_task)
        return;
    const auto task = createParseTask();
    if (!task)
        return;
    m_task = task;
    if (shouldPublishProgress()) {
        connect(task, &Task::statusUpdated, this, [this, task](const TaskStatus &status) {
            if (task == m_task && !m_terminal)
                publishProgress(status);
        });
    }
    connect(task, &Task::finished, this, [this, task] { handleTaskFinished(task); });
    connect(task, &Task::finished, task, &QObject::deleteLater);
    taskManager->addAndStartTask(task);
}

void ProjectLoadSessionBase::handleTaskFinished(Task *task) {
    if (taskManager->tasks().contains(task))
        taskManager->removeTask(task);
    if (task != m_task || m_terminal)
        return;
    m_task = nullptr;

    if (task->terminated()) {
        emitCanceled();
        return;
    }

    handleParseResult(task);
}

void ProjectLoadSessionBase::requestReprocess() {
    if (m_terminal)
        return;
    detachReprocessTask();
    const auto generation = ++m_reprocessGeneration;
    const auto task = createReprocessTask();
    if (!task)
        return;
    m_reprocessTask = task;
    connect(task, &Task::finished, this,
            [this, generation, task] { handleReprocessFinished(generation, task); });
    connect(task, &Task::finished, task, &QObject::deleteLater);
    taskManager->addAndStartTask(task);
}

void ProjectLoadSessionBase::handleReprocessFinished(const quint64 generation, Task *task) {
    if (taskManager->tasks().contains(task))
        taskManager->removeTask(task);
    if (task != m_reprocessTask || generation != m_reprocessGeneration || m_terminal)
        return;
    m_reprocessTask = nullptr;
    if (task->terminated())
        return;

    handleReprocessResult(task);
}

void ProjectLoadSessionBase::detachTask() {
    if (!m_task)
        return;
    const auto task = m_task;
    m_task = nullptr;
    taskManager->terminateTask(task);
    if (taskManager->tasks().contains(task))
        taskManager->removeTask(task);
}

void ProjectLoadSessionBase::detachReprocessTask() {
    if (!m_reprocessTask)
        return;
    const auto task = m_reprocessTask;
    m_reprocessTask = nullptr;
    taskManager->terminateTask(task);
    if (taskManager->tasks().contains(task))
        taskManager->removeTask(task);
}

void ProjectLoadSessionBase::publishProgress(const TaskStatus &status) {
    emit progressChanged({status.title, status.message, status.minimum, status.maximum,
                          status.progress, status.isIndetermine});
}

void ProjectLoadSessionBase::finishWithResult(PreparedProject result) {
    if (m_terminal)
        return;
    m_result = std::move(result);
    m_terminal = true;
    emit ready();
}

void ProjectLoadSessionBase::fail(const ProjectOperationError &error) {
    if (m_terminal)
        return;
    m_terminal = true;
    emit failed(error);
}

void ProjectLoadSessionBase::emitCanceled() {
    if (m_terminal)
        return;
    m_terminal = true;
    emit canceled();
}
