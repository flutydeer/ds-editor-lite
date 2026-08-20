#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QAction>
#include <QMainWindow>
#include <QPointer>
#include <QTimer>

#include "Global/AppOptionsGlobal.h"
#include "Interface/IMainWindow.h"
#include "Interface/IEditorView.h"
#include "Controller/DocumentWorkflow/IDocumentWorkflowUi.h"
#include "UI/Views/BottomPanelView.h"


class QSplitter;
class MainTitleBar;
class MainMenuView;
class TaskDialog;
class LogWindow;
class TrackEditorView;
class ClipEditorView;
class EmbeddedModalHost;
class AppOptionsDialog;
class QAction;
class QShortcut;

namespace QWK {
    class WidgetWindowAgent;
}

class MainWindow final : public QMainWindow,
                         public IMainWindow,
                         public IEditorView,
                         public IDocumentWorkflowUi {
    Q_OBJECT

public:
    explicit MainWindow();
    ~MainWindow() override;
    void updateWindowTitle() override;
    void quit() override;
    void restart() override;
    [[nodiscard]] EditorViewState captureEditorViewState() const override;
    bool restoreEditorViewState(const EditorViewState &state) override;
    bool centerTrackPanelAt(double tick, double trackIndex) override;
    bool setTrackPanelScale(double horizontalScale, double verticalScale) override;
    bool setEditorPanelVisibility(bool trackPanelVisible, bool bottomPanelVisible) override;
    bool showBottomPanelPage(const QString &pageId) override;
    bool centerPianoRollAt(double tick, double keyIndex) override;
    bool setPianoRollScale(double horizontalScale, double verticalScale) override;
    bool setPianoRollEditMode(EditorViewGlobal::PianoRollEditMode mode) override;
    void refreshActiveClipTrackPresentation() override;
    void previewActiveClipTrackColor(int colorIndex) override;
    [[nodiscard]] HistoryFocusVisibility focusVisibility(const HistoryFocus &focus) const override;
    bool revealFocus(const HistoryFocus &focus) override;
    bool finalizeFocus(const HistoryFocus &focus) override;
    void clearFocusPreview() override;
    void updateDiagnosticFilter();
    void updateLogWindowVisible();
    void updatePanelDetachEnabled();
    QWidget *documentWorkflowParentWidget() override;
    SaveDecision askDocumentSaveDecision() override;
    QString chooseDocumentSavePath(const QString &suggestedPath) override;
    bool confirmOpenWithoutPackageMetadata() override;
    void showDocumentWorkflowError(const ProjectOperationError &error) override;
    void showDocumentWorkflowBusy() override;
#if defined(WITH_DIRECT_MANIPULATION)
    void registerDirectManipulation();
    void unregisterDirectManipulation();
#endif

public slots:
    void onAllDone();

    // 内嵌式设置面板（EmbeddedModalHost 承载，带遮罩/动画）
    void openAppOptions(AppOptionsGlobal::Option option);
    void closeAppOptions();

protected:
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;

private slots:
    void onSplitterMoved(int pos, int index);
    void detachBottomPanel();
    void attachBottomPanel();

private:
    bool navigateToFocus(const HistoryFocus &focus, bool animated);
    void updatePanelVisibilityState(bool trackPanelVisible, bool bottomPanelVisible);
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    static void emulateLeaveEvent(QWidget *widget);
    static void restartApp();
    void updateShutdownBlockReason();

    bool m_restartRequested = false;
    bool m_isCloseRequested = false;
    bool m_isAllDone = false;
    bool m_isDirectManipulationRegistered = false;
    bool m_documentCloseApproved = false;

    MainTitleBar *m_titleBar;
    MainMenuView *m_mainMenu = nullptr;
    TrackEditorView *m_trackEditorView;
    BottomPanelView *m_bottomPanelView;
    QSplitter *m_splitter;
    QByteArray m_splitterState;

    QTimer m_waitDoneDialogDelayTimer;
    TaskDialog *m_waitDoneDialog = nullptr;

    bool m_bottomPanelDetached = false;
    bool m_useNativeFrame = false;
    QByteArray m_detachSplitterState;
    QRect m_detachedWindowGeometry;
    QWK::WidgetWindowAgent *m_detachedAgent = nullptr;
    QObject *m_eventDiagFilter = nullptr;
    LogWindow *m_logWindow = nullptr;

    // Embedded modal: backdrop overlay plus the settings panel.
    EmbeddedModalHost *m_modalHost = nullptr;
    AppOptionsDialog *m_appOptionsDialog = nullptr;
    // Actions/shortcuts of the background that are disabled while the modal is
    // open; restored on close. Stored as QPointer so that actions destroyed
    // while the modal is open (e.g. the track-control view rebuilding its
    // injected mix-preset actions in response to an option change) are
    // automatically dropped instead of dangling.
    QList<QPointer<QAction>> m_suspendedActions;
    QList<QShortcut *> m_suspendedShortcuts;
    // Focus holder before the modal opened. Restored on close: on Windows,
    // wheel events are delivered to the focus widget rather than the widget
    // under the cursor, so focus must live inside the modal for scrolling to
    // reach the settings pages.
    QPointer<QWidget> m_focusBeforeModal;

    void suspendBackgroundInteraction();
    void restoreBackgroundInteraction();
};



#endif // MAINWINDOW_H
