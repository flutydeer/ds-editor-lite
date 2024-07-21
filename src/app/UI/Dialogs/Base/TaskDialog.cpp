//
// Created by fluty on 2024/7/13.
//

#include <QHBoxLayout>
#include <QCloseEvent>

#include "TaskDialog.h"
#include "Modules/Task/Task.h"
#include "Modules/Task/TaskManager.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/ProgressIndicator.h"

TaskDialog::TaskDialog(Task *task, bool cancellable, bool canHide, QWidget *parent)
    : Dialog(parent), m_task(task), m_canHide(canHide) {
    setModal(true);
    setMinimumWidth(360);
    m_progressBar = new ProgressIndicator(this);
    m_progressBar->setIndeterminate(true);
    if (cancellable) {
        m_btnCancel = new Button(tr("Cancel"), this);
        setNegativeButton(m_btnCancel);
        connect(m_btnCancel, &Button::clicked, this, &TaskDialog::onTerminateButtonClicked);
    }
    if (canHide) {
        m_btnHide = new Button(tr("Hide"), this);
        setPositiveButton(m_btnHide);
        connect(m_btnHide, &Button::clicked, this, &TaskDialog::accept);
    } else {
        setWindowFlags((windowFlags() & ~Qt::WindowCloseButtonHint));
    }
    auto mainLayout = new QHBoxLayout;
    mainLayout->addWidget(m_progressBar);
    mainLayout->setContentsMargins({});
    body()->setLayout(mainLayout);

    if (!m_task)
        return;

    connect(m_task, &Task::statusUpdated, this, &TaskDialog::onStatusUpdated);
    connect(m_task, &Task::finished, this, [this] { accept(); });
}
void TaskDialog::forceClose() {
    m_canHide = true;
    accept();
}
void TaskDialog::closeEvent(QCloseEvent *event) {
    if (m_canHide)
        Dialog::closeEvent(event);
    else
        event->ignore();
}
void TaskDialog::onTerminateButtonClicked() {
    if (m_task)
        taskManager->terminateTask(m_task);
}
void TaskDialog::onStatusUpdated(const TaskStatus &status) {
    setTitle(status.title);
    setMessage(status.message);
    m_progressBar->setMinimum(status.minimum);
    m_progressBar->setMaximum(status.maximum);
    m_progressBar->setValue(status.progress);
    m_progressBar->setIndeterminate(status.isIndetermine);
    m_progressBar->setTaskStatus(status.runningStatus);
}