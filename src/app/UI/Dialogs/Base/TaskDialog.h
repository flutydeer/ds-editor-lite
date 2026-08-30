#ifndef TASKDIALOG_H
#define TASKDIALOG_H

#include "ProgressDialog.h"

#include <functional>

class TaskStatus;
class Task;

class TaskDialog : public ProgressDialog {
    Q_OBJECT

public:
    explicit TaskDialog(Task *task = nullptr, bool cancellable = true, bool canHide = true,
                        QWidget *parent = nullptr);

    void setCancelCallback(std::function<void()> callback);

private:
    void onCanceled() override;

private slots:
    void onStatusUpdated(const TaskStatus &status) const;

private:
    Task *m_task;
    std::function<void()> m_cancelCallback;
};

#endif // TASKDIALOG_H
