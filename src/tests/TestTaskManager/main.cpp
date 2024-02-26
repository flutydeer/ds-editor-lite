//
// Created by fluty on 24-2-26.
//

#include <QApplication>
#include <QStyleFactory>
#include <QMainWindow>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "TaskManager.h"
#include "TestTask.h"
#include "ProgressIndicator.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    auto style = QStyleFactory::create("fusion");
    QApplication::setStyle(style);

    auto f = QFont();
    f.setHintingPreference(QFont::PreferNoHinting);
    QApplication::setFont(f);

    ProgressIndicator progressBar;
    QPushButton btnTerminate("Terminate");
    QVBoxLayout mainLayout;
    mainLayout.addWidget(&progressBar);
    mainLayout.addWidget(&btnTerminate);
    QWidget mainWidget;
    mainWidget.setLayout(&mainLayout);

    QMainWindow w;
    w.setCentralWidget(&mainWidget);
    w.resize(360, 120);
    w.show();

    auto taskManager = TaskManager::instance();
    auto task = new TestTask("miao");
    QObject::connect(task, &ITask::progressUpdated, &progressBar, &ProgressIndicator::setValue);
    QObject::connect(task, &ITask::finished, [=] {
        qDebug() << "Task finished" << task->resultData();
    });
    QObject::connect(&btnTerminate, &QPushButton::clicked, [&] {
        taskManager->terminateAllTasks();
    });
    taskManager->addTask(task);
    taskManager->startAllTasks();

    return QApplication::exec();
}