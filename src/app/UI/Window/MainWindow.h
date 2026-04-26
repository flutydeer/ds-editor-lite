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
#include "UI/Views/BottomPanelView.h"


class QSplitter;
class MainTitleBar;
class MainMenuView;
class TaskDialog;
class ProgressIndicator;
class QLabel;
class TrackEditorView;
class ClipEditorView;

namespace QWK {
class WidgetWindowAgent;
}

class MainWindow final : public QMainWindow, public IMainWindow {
    Q_OBJECT

public:
    explicit MainWindow();
    ~MainWindow() override;
    void updateWindowTitle() override;
    bool askSaveChanges() override;
    void quit() override;
    void restart() override;
    void setTrackAndClipPanelCollapsed(bool trackCollapsed, bool clipCollapsed) override;
#if defined(WITH_DIRECT_MANIPULATION)
    void registerDirectManipulation();
    void unregisterDirectManipulation();
#endif

public slots:
    void onAllDone();
    void onTaskChanged(TaskManager::TaskChangeType type, Task *task, qsizetype index);
    void onTaskStatusChanged(const TaskStatus &status);

protected:
    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;

private slots:
    bool onSave();
    bool onSaveAs();
    void onSplitterMoved(int pos, int index) const;
    void detachBottomPanel();
    void attachBottomPanel();

private:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    static void emulateLeaveEvent(QWidget *widget);
    static void restartApp();

    bool m_restartRequested = false;
    bool m_isCloseRequested = false;
    bool m_isAllDone = false;
    bool m_isDirectManipulationRegistered = false;
    bool m_isSaveChangesHandled = false;

    MainTitleBar *m_titleBar;
    MainMenuView *m_mainMenu = nullptr;
    TrackEditorView *m_trackEditorView;
    BottomPanelView *m_bottomPanelView;
    QSplitter *m_splitter;
    QByteArray m_splitterState;

    QLabel *m_lbTaskTitle = nullptr;
    ProgressIndicator *m_statusProgressBar = nullptr;

    QTimer m_waitDoneDialogDelayTimer;
    TaskDialog *m_waitDoneDialog = nullptr;

    bool m_bottomPanelDetached = false;
    bool m_useNativeFrame = false;
    QByteArray m_detachSplitterState;
    QRect m_detachedWindowGeometry;
    QWK::WidgetWindowAgent *m_detachedAgent = nullptr;

    // int m_noteIndex = 0;
};



#endif // MAINWINDOW_H
