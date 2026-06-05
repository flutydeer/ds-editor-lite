//
// Created by fluty on 2024/7/13.
//

#include "TaskDialog.h"

#include "Modules/Task/Task.h"
#include "Modules/Task/TaskManager.h"

TaskDialog::TaskDialog(Task *task, const bool cancellable, const bool canHide, QWidget *parent)
    : ProgressDialog(cancellable, canHide, parent), m_task(task) {
    if (!m_task)
        return;

    connect(m_task, &Task::statusUpdated, this, &TaskDialog::onStatusUpdated);
    connect(m_task, &Task::finished, this, [this] { accept(); });
}

void TaskDialog::onCanceled() {
    if (m_task)
        taskManager->terminateTask(m_task);
    ProgressDialog::onCanceled();
}

void TaskDialog::onStatusUpdated(const TaskStatus &status) const {
    setTitle(status.title);
    setMessage(status.message);
    setProgressRange(status.minimum, status.maximum);
    setProgressValue(status.progress);
    setProgressIndeterminate(status.isIndetermine);
    setProgressStatus(status.runningStatus);
}
