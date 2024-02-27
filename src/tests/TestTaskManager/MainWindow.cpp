//
// Created by fluty on 24-2-28.
//

#include "MainWindow.h"

#include <QCloseEvent>
#include <QPushButton>
#include <QVBoxLayout>

#include "TaskManager.h"
#include "TestTask.h"
#include "ProgressIndicator.h"



MainWindow::MainWindow() {
    auto progressBar = new ProgressIndicator;
    auto btnTerminate = new QPushButton("Terminate");
    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(progressBar);
    mainLayout->addWidget(btnTerminate);
    auto mainWidget = new QWidget;
    mainWidget->setLayout(mainLayout);
    auto taskManager = TaskManager::instance();
    auto task = new TestTask("miao");
    QObject::connect(task, &ITask::statusUpdated, this,
                     [=](int progress, ITask::TaskStatus status, bool isIndeterminate) {
                         progressBar->setValue(progress);
                         if (status == ITask::Normal)
                             progressBar->setTaskStatus(ProgressIndicator::Normal);
                         else
                             progressBar->setTaskStatus(ProgressIndicator::Error);
                         progressBar->setIndeterminate(isIndeterminate);
                     });
    QObject::connect(task, &ITask::finished,
                     [=] { qDebug() << "Task finished" << task->resultData(); });
    // QObject::connect(task, &ITask::finished, [=](bool terminate) {
    //     qDebug() << "Task finished" << task->resultData();
    //     if (terminate) {
    //         progressBar->setIndeterminate(false);
    //         progressBar->setValue(0);
    //     }
    // });
    QObject::connect(btnTerminate, &QPushButton::clicked, taskManager,
                     &TaskManager::terminateAllTasks);
    QObject::connect(taskManager, &TaskManager::allDone, this, &MainWindow::onAllDone);
    taskManager->addTask(task);
    taskManager->startAllTasks();

    setCentralWidget(mainWidget);
}


void MainWindow::onAllDone() {
    if (m_isCloseRequested) {
        m_isAllDone = true;
        close();
    }
}
void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_isAllDone) {
        QMainWindow::closeEvent(event);
    } else if (m_isCloseRequested) {
        qDebug() << "Waiting for all tasks done...";
        event->ignore();
    } else {
        m_isCloseRequested = true;
        qDebug() << "Ternimating tasks...";
        auto taskManager = TaskManager::instance();
        taskManager->terminateAllTasks();
        auto thread = new QThread;
        taskManager->moveToThread(thread);
        connect(thread, &QThread::started, taskManager, &TaskManager::wait);
        thread->start();
        event->ignore();
    }
    // taskManager->wait();
    // QMainWindow::closeEvent(event);
}