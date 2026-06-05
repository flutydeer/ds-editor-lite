//
// Created by fluty on 2024/7/13.
//

#ifndef TASKDIALOG_H
#define TASKDIALOG_H

#include "ProgressDialog.h"

class TaskStatus;
class Task;

class TaskDialog : public ProgressDialog {
    Q_OBJECT

public:
    explicit TaskDialog(Task *task = nullptr, bool cancellable = true, bool canHide = true,
                        QWidget *parent = nullptr);

private:
    void onCanceled() override;

private slots:
    void onStatusUpdated(const TaskStatus &status) const;

private:
    Task *m_task;
};

#endif // TASKDIALOG_H
