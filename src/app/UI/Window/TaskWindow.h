//
// Created by fluty on 24-3-19.
//

#ifndef TASKWINDOW_H
#define TASKWINDOW_H

#include "Base/Window.h"
#include "Modules/Task/TaskManager.h"


class QListWidgetItem;
class QListWidget;
class TaskView;
class TaskWindow : public Window {
Q_OBJECT

public:
    explicit TaskWindow(QWidget *parent = nullptr);

public slots:
    void onTaskChanged(TaskManager::TaskChangeType type, Task *task, qsizetype index);

private:
    QListWidget *m_taskList{};

    void addTaskToView(Task *task);
    void removeTaskFromView(qsizetype index);
};



#endif //TASKWINDOW_H
