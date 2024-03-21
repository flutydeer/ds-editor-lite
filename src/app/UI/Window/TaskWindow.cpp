//
// Created by fluty on 24-3-19.
//

#include "TaskWindow.h"

#include <QListWidget>
#include <QVBoxLayout>

#include "UI/Controls/TaskView.h"

TaskWindow::TaskWindow(QWidget *parent) : Window(parent) {
    m_taskList = new QListWidget;
    m_taskList->setStyleSheet("QListWidget { background: transparent; border: none; "
                                     "border-right: 1px solid #202020; outline:0px;"
                                     "border-top: 1px solid #202020;"
                                     "margin-bottom: 16px } "
                                     "QListWidget::item:hover { background: #05FFFFFF }"
                                     "QListWidget::item:selected { background: #10FFFFFF }");

    connect(TaskManager::instance(), &TaskManager::taskChanged, this, &TaskWindow::onTaskChanged);

    setWindowFlags(Qt::Tool/* | Qt::WindowStaysOnTopHint*/);
    auto mainLayout = new QVBoxLayout;
    mainLayout->setContentsMargins({});
    mainLayout->addWidget(m_taskList);
    setLayout(mainLayout);
    resize(360, 480);
}
void TaskWindow::onTaskChanged(TaskManager::TaskChangeType type, ITask *task, qsizetype index) {
    if (type == TaskManager::Added)
        addTaskToView(task);
    else if (type == TaskManager::Removed)
        removeTaskFromView(index);
}
void TaskWindow::addTaskToView(ITask *task) {
    qDebug() << "TaskWindow::addTaskToView";
    auto item = new QListWidgetItem;
    auto taskView = new TaskView(task->status());
    item->setSizeHint(QSize(m_taskList->width() - 4, 72));
    m_taskList->addItem(item);
    m_taskList->setItemWidget(item, taskView);

    connect(task, &ITask::statusUpdated, taskView, &TaskView::onTaskStatusChanged);
}
void TaskWindow::removeTaskFromView(qsizetype index) {
    qDebug() << "TaskWindow::removeTaskFromView";
    auto item = m_taskList->takeItem(index);
    auto widget = m_taskList->itemWidget(item);
    m_taskList->removeItemWidget(item);
    delete widget;
    delete item;
}