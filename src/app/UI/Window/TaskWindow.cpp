//
// Created by fluty on 24-3-19.
//

#include "TaskWindow.h"

#include <QListWidget>
#include <QVBoxLayout>
#include <QFile>

#include "UI/Controls/TaskView.h"

TaskWindow::TaskWindow(QWidget *parent) : Window(parent) {
    QString qssBase;
    auto qssFile = QFile(":theme/lite-dark.qss");
    if (qssFile.open(QIODevice::ReadOnly)) {
        qssBase = qssFile.readAll();
        qssFile.close();
    }
    this->setStyleSheet(QString("Window { background: #232425; }") + qssBase);
#ifdef Q_OS_WIN
    bool micaOn = true;
    auto version = QSysInfo::productVersion();
    if (micaOn && version == "11") {
        // make window transparent
        this->setStyleSheet(QString("Window { background: transparent }") + qssBase);
    }
#elif defined(Q_OS_MAC)
    this->setStyleSheet(QString("indow { background: transparent }") + qssBase);
#endif

    setWindowTitle("Background Tasks - DS Editor Lite");
    m_taskList = new QListWidget;
    m_taskList->setStyleSheet("QListWidget { background: transparent; border: none; "
                              "border-right: 1px solid #202020; outline:0px;"
                              "border-top: 1px solid #202020;"
                              "margin-bottom: 16px } "
                              "QListWidget::item:hover { background: #05FFFFFF }"
                              "QListWidget::item:selected { background: #10FFFFFF }");

    for (auto task : taskManager->tasks())
        addTaskToView(task);

    connect(taskManager, &TaskManager::taskChanged, this, &TaskWindow::onTaskChanged);

    setWindowFlags(Qt::Tool /* | Qt::WindowStaysOnTopHint*/);
    auto mainLayout = new QVBoxLayout;
    mainLayout->setContentsMargins({});
    mainLayout->addWidget(m_taskList);
    setLayout(mainLayout);
    resize(360, 480);
}

void TaskWindow::onTaskChanged(TaskManager::TaskChangeType type, Task *task, qsizetype index) {
    if (type == TaskManager::Added)
        addTaskToView(task);
    else if (type == TaskManager::Removed)
        removeTaskFromView(index);
}

void TaskWindow::addTaskToView(Task *task) {
    qDebug() << "TaskWindow::addTaskToView";
    auto item = new QListWidgetItem;
    auto taskView = new TaskView(task->status());
    item->setSizeHint(QSize(m_taskList->width() - 4, 72));
    m_taskList->addItem(item);
    m_taskList->setItemWidget(item, taskView);

    connect(task, &Task::statusUpdated, taskView, &TaskView::onTaskStatusChanged);
}

void TaskWindow::removeTaskFromView(qsizetype index) {
    qDebug() << "TaskWindow::removeTaskFromView";
    auto item = m_taskList->takeItem(index);
    auto widget = m_taskList->itemWidget(item);
    m_taskList->removeItemWidget(item);
    delete widget;
    delete item;
}