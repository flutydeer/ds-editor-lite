#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QAction>
#include <QMainWindow>
#include <QTimer>

#include "Global/AppOptionsGlobal.h"
#include "Interface/IMainWindow.h"
#include "Interface/IEditorView.h"
#include "Controller/DocumentWorkflow/IDocumentWorkflowUi.h"
#include "UI/Views/BottomPanelView.h"


class QSplitter;
class MainTitleBar;
class MainMenuView;
class DetachedPanelWindow;
class TaskDialog;
class LogWindow;
class TrackEditorView;
class ClipEditorView;
class QAction;

class MainWindow final : public QMainWindow,
                         public IMainWindow,
                         public IEditorView,
                         public IDocumentWorkflowUi {
    Q_OBJECT

public:
    explicit MainWindow();
    ~MainWindow() override;
    void updateWindowTitle() override;
    [[nodiscard]] EditorViewState captureEditorViewState() const override;
    bool restoreEditorViewState(const EditorViewState &state) override;
    bool resetDocumentScopedViewState() override;
    bool centerTrackPanelAt(double tick, double trackIndex) override;
    bool setTrackPanelScale(double horizontalScale, double verticalScale) override;
    bool setTrackPanelViewport(const TrackPanelViewState &state) override;
    bool setEditorPanelVisibility(bool trackPanelVisible, bool bottomPanelVisible) override;
    bool showBottomPanelPage(const QString &pageId) override;
    bool showEditorRegion(EditorViewGlobal::Region region) override;
    bool focusEditorRegion(EditorViewGlobal::Region region) override;
    bool centerPianoRollAt(double tick, double keyIndex) override;
    bool setPianoRollScale(double horizontalScale, double verticalScale) override;
    bool setClipEditorTimeViewport(double centerTick, double horizontalScale) override;
    bool setPianoRollPitchViewport(double centerKeyIndex, double verticalScale) override;
    bool setPianoRollEditMode(EditorViewGlobal::PianoRollEditMode mode) override;
    bool setParameterForeground(ParamInfo::Name name) override;
    bool setParameterBackground(ParamInfo::Name name) override;
    bool swapParameters() override;
    bool setParameterEditMode(EditorViewGlobal::ParameterEditMode mode) override;
    bool setParameterValueViewport(double centerRatio, double verticalScale) override;
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

public slots:
    void onAllDone();

    // Opens the standalone options dialog parented to the window whose menu
    // triggered the action (a detached panel window owns its own menu bar).
    void openAppOptions(AppOptionsGlobal::Option option, QWidget *requestingWindow = nullptr);

protected:
    bool event(QEvent *event) override;
    void changeEvent(QEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;

private slots:
    void onSplitterMoved(int pos, int index);
    void detachBottomPanel();
    void attachBottomPanel();

private:
    bool navigateToFocus(const HistoryFocus &focus, bool animated);
    [[nodiscard]] EditorViewGlobal::Region focusedEditorRegion() const;
    [[nodiscard]] bool editorFocusControlBlocked() const;
    void updatePanelVisibilityState(bool trackPanelVisible, bool bottomPanelVisible);
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    static void emulateLeaveEvent(QWidget *widget);
    static void restartApp();
    void updateShutdownBlockReason();
    [[nodiscard]] bool mainMenuInWindow() const;

    bool m_restartRequested = false;
    bool m_isCloseRequested = false;
    bool m_isAllDone = false;
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
    DetachedPanelWindow *m_detachedWindow = nullptr;
    QObject *m_eventDiagFilter = nullptr;
    LogWindow *m_logWindow = nullptr;
};



#endif // MAINWINDOW_H
