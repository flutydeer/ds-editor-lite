#include "TaskWindow.h"
#include "AppContext.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"

#include <QListWidget>
#include <QVBoxLayout>
#include <QFile>

#include <lite/GUI/Controls/TaskView.h>

#include <QApplication>
#include <QEvent>

TaskWindow::TaskWindow(QWidget *parent) : Window(parent) {
    retranslateUi();
    m_taskList = new QListWidget;
    m_taskList->setObjectName("taskList");

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

void TaskWindow::changeEvent(QEvent *event) {
    Window::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
}

void TaskWindow::retranslateUi() {
    setWindowTitle(tr("Background Tasks - %1").arg(QGuiApplication::applicationDisplayName()));
}

void TaskWindow::onTaskChanged(TaskManager::TaskChangeType type, Task *task, qsizetype index) {
    if (type == TaskManager::Added)
        addTaskToView(task);
    else if (type == TaskManager::Removed)
        removeTaskFromView(index);
}

void TaskWindow::addTaskToView(Task *task) {
    const SingletonRegistry::Scope scope(ObjectContext::of(task));
    const auto *workflow = AppContext::instance<DocumentWorkflowController>();
    const auto name = workflow ? workflow->projectName() : QString();
    const auto labeled = [name](TaskStatus status) {
        if (!name.isEmpty())
            status.title = QStringLiteral("%1 — %2").arg(name, status.title);
        return status;
    };
    auto item = new QListWidgetItem;
    auto taskView = new TaskView(labeled(task->status()), this);
    item->setSizeHint(QSize(m_taskList->width() - 4, 72));
    m_taskList->addItem(item);
    m_taskList->setItemWidget(item, taskView);

    connect(task, &Task::statusUpdated, taskView, [taskView, labeled](const TaskStatus &status) {
        taskView->onTaskStatusChanged(labeled(status));
    });
}

void TaskWindow::removeTaskFromView(qsizetype index) {
    auto item = m_taskList->takeItem(index);
    auto widget = m_taskList->itemWidget(item);
    m_taskList->removeItemWidget(item);
    delete widget;
    delete item;
}
