//
// Created by fluty on 2024/1/31.
//

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "Modules/Task/TaskManager.h"
#include "Modules/Task/ITask.h"

class ProgressIndicator;
class QLabel;
class TracksView;
class ClipEditorView;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow();

public slots:
    void onAllDone();
    void onTaskChanged(TaskManager::TaskChangeType type, ITask *task, qsizetype index);
    void onTaskStatusChanged(const TaskStatus &status);

private:
    void closeEvent(QCloseEvent *event) override;

    bool m_isCloseRequested = false;
    bool m_isAllDone = false;

    TracksView *m_tracksView;
    ClipEditorView *m_clipEditView;

    QLabel *m_lbTaskTitle;
    ProgressIndicator *m_progressBar;
    ITask *m_firstask = nullptr;

    // int m_noteIndex = 0;
};



#endif // MAINWINDOW_H
