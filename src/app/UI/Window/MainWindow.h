//
// Created by fluty on 2024/1/31.
//

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>

#include "Interface/IMainWindow.h"
#include "Modules/Task/TaskManager.h"
#include "Modules/Task/Task.h"


class MainTitleBar;
class MainMenuView;
class TaskDialog;
class ProgressIndicator;
class QLabel;
class TrackEditorView;
class ClipEditorView;

class MainWindow final : public QMainWindow, public IMainWindow {
    Q_OBJECT

public:
    explicit MainWindow();
    void updateWindowTitle() override;
    bool askSaveChanges() override;

public slots:
    void onAllDone();
    void onTaskChanged(TaskManager::TaskChangeType type, Task *task, qsizetype index);
    void onTaskStatusChanged(const TaskStatus &status);

private slots:
    bool onSave();
    bool onSaveAs();

private:
    void closeEvent(QCloseEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

    bool m_isCloseRequested = false;
    bool m_isAllDone = false;

    MainTitleBar *m_titleBar;
    MainMenuView *m_mainMenu = nullptr;
    TrackEditorView *m_trackEditorView;
    ClipEditorView *m_clipEditView;

    QLabel *m_lbTaskTitle;
    ProgressIndicator *m_progressBar;

    QTimer m_waitDoneDialogDelayTimer;
    TaskDialog *m_waitDoneDialog = nullptr;

    // int m_noteIndex = 0;
};



#endif // MAINWINDOW_H
