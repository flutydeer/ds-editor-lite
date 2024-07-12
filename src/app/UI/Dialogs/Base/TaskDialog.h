//
// Created by fluty on 2024/7/13.
//

#ifndef TASKDIALOG_H
#define TASKDIALOG_H

#include "Dialog.h"

class TaskStatus;
class Task;
class ProgressIndicator;
class Button;
class TaskDialog : public Dialog {
    Q_OBJECT

public:
    explicit TaskDialog(Task *task = nullptr, bool cancellable = true, bool canHide = true, QWidget *parent = nullptr);

private slots:
    void onTerminateButtonClicked();
    void onStatusUpdated(const TaskStatus &status);

private:
    Task *m_task;
    bool m_cancellable;
    Button *m_btnHide;
    Button *m_btnCancel;
    ProgressIndicator *m_progressBar;
};



#endif // TASKDIALOG_H
