#include "MainWindow.h"

#ifdef Q_OS_WIN
#  include <Windows.h>
#  include <WinUser.h>
#endif

#include "Controller/AppController.h"
#include "Controller/EditorViewController.h"
#include "Controller/AudioDecodingController.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Controller/TrackController.h"
#include "Controller/UndoRedoController.h"
#include "Modules/Import/DocumentImportController.h"
#include "Modules/Import/ExternalFileClassifier.h"
#include <lite/ProjectModel/AppModel/AppModel.h>
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include <lite/History/HistoryManager.h>
#include <lite/PackageManager/PackageManager.h>
#include <lite/Tasking/TaskManager.h>
#include <lite/GUI/Controls/AccentButton.h>
#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/SystemWindowButton.h>
#include <lite/GUI/Controls/SilentSplitter.h>
#include <lite/GUI/Controls/Toast.h>
#include <lite/GUI/Theme/ThemeManager.h>
#include <lite/GUI/Theme/ThemeLoader.h>
#include "UI/Dialogs/Base/MessageDialog.h"
#include "UI/Dialogs/Base/TaskDialog.h"
#include "UI/Dialogs/Options/AppOptionsDialog.h"
#include "UI/Dialogs/ResourceCheck/AudioResourcePage.h"
#include "UI/Dialogs/ResourceCheck/ResourceCheckDialog.h"
#include "UI/Views/BottomPanelView.h"
#include "UI/Views/ClipEditor/ClipEditorView.h"
#include "UI/Views/Common/TabPanelTitleBar.h"
#include "UI/Views/MainTitleBar/TitleBarComboBox.h"
#include "UI/Views/MainTitleBar/FilePopupWidget.h"
#include "UI/Window/EmbeddedModalHost.h"
#include "UI/Window/EventDiagFilter.h"
#include "UI/Window/LogWindow.h"

#include <QStackedWidget>
#include <QTabBar>
#include <QTimer>
#include "UI/Views/MainTitleBar/ActionButtonsView.h"
#include "UI/Views/MainTitleBar/MainMenuView.h"
#include "UI/Views/MainTitleBar/MainTitleBar.h"
#include "UI/Views/MainTitleBar/PlaybackView.h"
#include "UI/Views/TrackEditor/TrackEditorView.h"
#include <lite/GUI/Utils/WindowFrameUtils.h>

#include <QApplication>
#include <QFileInfo>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMimeData>
#include <QProcess>
#include <QShortcut>
#include <QSplitter>
#include <QWKWidgets/widgetwindowagent.h>

#include <cmath>
#include <utility>

#if defined(WITH_DIRECT_MANIPULATION)
#  include <QWDMHCore/DirectManipulationSystem.h>
#endif

MainWindow::MainWindow() {
    setAcceptDrops(true);

    m_useNativeFrame = appOptions->appearance()->useNativeFrame;
    auto useNativeFrame = m_useNativeFrame;
    m_mainMenu = new MainMenuView(this);
    m_titleBar = new MainTitleBar(m_mainMenu, this, useNativeFrame);

    if (!useNativeFrame) {
        auto agent = new QWK::WidgetWindowAgent(this);
        agent->setup(this);
        agent->setTitleBar(m_titleBar);
        agent->setSystemButton(QWK::WindowAgentBase::Minimize, m_titleBar->minimizeButton());
        agent->setSystemButton(QWK::WindowAgentBase::Maximize, m_titleBar->maximizeButton());
        agent->setSystemButton(QWK::WindowAgentBase::Close, m_titleBar->closeButton());
        agent->setHitTestVisible(m_titleBar->menuView());
        agent->setHitTestVisible(m_titleBar->actionButtonsView());
        agent->setHitTestVisible(m_titleBar->playbackView());
        agent->setHitTestVisible(m_titleBar->titleComboBox());

        connect(m_titleBar, &MainTitleBar::minimizeTriggered, this, &MainMenuView::showMinimized);
        connect(m_titleBar, &MainTitleBar::maximizeTriggered, this, [&](bool max) {
            if (max)
                showMaximized();
            else
                showNormal();
            emulateLeaveEvent(m_titleBar->maximizeButton());
        });
        connect(m_titleBar, &MainTitleBar::closeTriggered, this, &MainWindow::close);
    }

    // Connect file popup actions
    auto *filePopup = m_titleBar->titleComboBox()->popupWidget();
    connect(filePopup, &FilePopupWidget::newProjectClicked, m_mainMenu->actionNew(),
            &QAction::trigger);
    connect(filePopup, &FilePopupWidget::openProjectClicked, m_mainMenu->actionOpen(),
            &QAction::trigger);
    connect(filePopup, &FilePopupWidget::openRecentProject, m_mainMenu,
            &MainMenuView::openRecentProject);
    installEventFilter(m_titleBar);

    ThemeManager::instance()->addStyleRoot(this);

    auto themeReloadShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F5), this);
    connect(themeReloadShortcut, &QShortcut::activated, this, [this] {
        if (!ThemeManager::instance()->reloadCurrentTheme()) {
            qWarning() << "Failed to reload theme:" << ThemeLoader::lastError();
            Toast::show(tr("Failed to reload theme"));
        } else {
            Toast::show(tr("Theme reloaded"));
        }
    });

    Dialog::setGlobalContext(this);
    Toast::setGlobalContext(this);
    appController->setMainWindow(this);
    documentWorkflowController->setUi(this);
    connect(documentWorkflowController, &DocumentWorkflowController::documentIdentityChanged, this,
            &MainWindow::updateWindowTitle);
    connect(historyManager, &HistoryManager::savePointChanged, this,
            &MainWindow::updateWindowTitle);
    connect(documentWorkflowController, &DocumentWorkflowController::terminationApproved, this,
            [this](const TerminationMode mode) {
                m_restartRequested = mode == TerminationMode::Restart;
                m_documentCloseApproved = true;
                QTimer::singleShot(0, this, [this] { close(); });
            });

    connect(appOptions, &AppOptions::optionsChanged, this,
            [this](const AppOptionsGlobal::Option option) {
                if (option == AppOptionsGlobal::DeveloperOptions || option == AppOptionsGlobal::All)
                    updateDiagnosticFilter();
                if (option == AppOptionsGlobal::DeveloperOptions || option == AppOptionsGlobal::All)
                    updatePanelDetachEnabled();
                if (option == AppOptionsGlobal::DeveloperOptions || option == AppOptionsGlobal::All)
                    updateLogWindowVisible();
            });
    updateDiagnosticFilter();
    updateLogWindowVisible();

    connect(taskManager, &TaskManager::allDone, this, &MainWindow::onAllDone);

    connect(audioDecodingController, &AudioDecodingController::resolveSessionFinished, this,
            [this](const QList<int> &missingClipIds, const QList<int> &unconfirmedClipIds, int) {
                if (missingClipIds.isEmpty() && unconfirmedClipIds.isEmpty())
                    return;
                const auto dialog = new ResourceCheckDialog(this);
                dialog->setAttribute(Qt::WA_DeleteOnClose);
                dialog->addPage(new AudioResourcePage(missingClipIds, unconfirmedClipIds));
                dialog->finalizePages();
                dialog->show();
            });

    connect(m_mainMenu->actionSave(), &QAction::triggered, documentWorkflowController,
            &DocumentWorkflowController::requestSave);
    connect(m_mainMenu->actionSaveAs(), &QAction::triggered, documentWorkflowController,
            &DocumentWorkflowController::requestSaveAs);

    m_trackEditorView = new TrackEditorView;
    m_bottomPanelView = new BottomPanelView(this);
    connect(m_bottomPanelView, &BottomPanelView::detachRequested, this,
            &MainWindow::detachBottomPanel);
    updatePanelDetachEnabled();

    m_splitter = new SilentSplitter;
    m_splitter->setOrientation(Qt::Vertical);
    m_splitter->addWidget(m_trackEditorView);
    m_splitter->addWidget(m_bottomPanelView);
    // 让轨道编辑器高度较小，剪辑编辑器高度较大，且在纵向拉伸窗口时优先拉伸钢琴卷帘
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 100);
    m_splitter->setContentsMargins(6, 0, 6, 0);
    connect(m_splitter, &QSplitter::splitterMoved, this, &MainWindow::onSplitterMoved);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(m_titleBar);
    mainLayout->addWidget(m_splitter);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 6);

    auto mainWidget = new QWidget;
    mainWidget->setLayout(mainLayout);

    this->setCentralWidget(mainWidget);
    editorViewController->setView(this);

    QTimer::singleShot(0, this, [this] {
        const auto sizes = m_splitter->sizes();
        if (sizes.size() >= 2 && sizes.at(0) > 0 && sizes.at(1) > 0)
            m_splitterState = m_splitter->saveState();
    });

    ThemeManager::instance()->addWindow(this);
#if defined(WITH_DIRECT_MANIPULATION)
    connect(appOptions, &AppOptions::optionsChanged, [&](AppOptionsGlobal::Option option) {
        if (option == AppOptionsGlobal::Option::Appearance) {
            // While the embedded options modal is open, DM must stay off: openAppOptions()
            // unregisters it and restoreBackgroundInteraction() re-registers on close. Any
            // Appearance change here (e.g. the animation level) would otherwise re-register
            // DM mid-modal and let it hijack the panel's wheel events.
            if (m_modalHost && m_modalHost->isOpen())
                return;
            if (appOptions->appearance()->enableDirectManipulation) {
                registerDirectManipulation();
            } else {
                unregisterDirectManipulation();
            }
        }
    });
#endif
    documentWorkflowController->initializeNewDocument();

    connect(undoRedoController, &UndoRedoController::focusNavigationRequested, this,
            [this](const bool undo) {
                Toast::show(undo ? tr("Press Undo again to apply")
                                 : tr("Press Redo again to apply"));
            });
}

MainWindow::~MainWindow() {
#ifdef Q_OS_WIN
    ShutdownBlockReasonDestroy(reinterpret_cast<HWND>(this->winId()));
#endif
    editorViewController->setView(nullptr);
    ThemeManager::instance()->removeWindow(this);
}

void MainWindow::updateDiagnosticFilter() {
    const bool enabled = appOptions->developer()->enableDiagnostics;
    if (enabled && !m_eventDiagFilter) {
        m_eventDiagFilter = new EventDiagFilter(this);
        qApp->installEventFilter(m_eventDiagFilter);
    } else if (!enabled && m_eventDiagFilter) {
        qApp->removeEventFilter(m_eventDiagFilter);
        delete m_eventDiagFilter;
        m_eventDiagFilter = nullptr;
    }
}

void MainWindow::updateLogWindowVisible() {
    const bool enabled = appOptions->developer()->showLogWindow;
    if (enabled) {
        // Create lazily on first use; keep instance (and its history) when hidden
        if (!m_logWindow)
            m_logWindow = new LogWindow(this);
        m_logWindow->show();
        m_logWindow->raise();
    } else if (m_logWindow) {
        m_logWindow->hide();
    }
}

void MainWindow::updatePanelDetachEnabled() {
    const bool enabled = appOptions->developer()->enablePanelDetach;
    if (!enabled && m_bottomPanelDetached)
        attachBottomPanel();
    m_bottomPanelView->titleBar()->setDetachButtonVisible(enabled);
}

void MainWindow::updateWindowTitle() {
    auto projectName = documentWorkflowController->projectName();
    auto saved = historyManager->isOnSavePoint();
    auto appName = qApp->applicationDisplayName();
    if (projectName.isNull() || projectName.isEmpty())
        setWindowTitle(appName);
    else {
        auto projectPath = documentWorkflowController->projectPath();
        auto displayName =
            projectPath.isEmpty() ? projectName : QFileInfo(projectPath).completeBaseName();
        auto indicator = saved ? "" : "● ";
        setWindowTitle(indicator + displayName);
    }
    updateShutdownBlockReason();
}

void MainWindow::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        updateWindowTitle();
    else if (event->type() == QEvent::ActivationChange && isActiveWindow() &&
             m_bottomPanelDetached) {
        editorViewController->activatePanelContext(
            m_trackEditorView->isVisible() ? AppGlobal::TracksEditor : AppGlobal::Generic);
    }
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    // Keep the floating modal overlay in sync (layout-less overlay, same
    // pattern as the OverlaySplitter grip).
    if (m_modalHost)
        m_modalHost->setGeometry(rect());
}

void MainWindow::openAppOptions(const AppOptionsGlobal::Option option) {
    // The embedded panel is experimental and opt-in (Developer Options ->
    // Embedded options dialog); the standalone dialog stays the default.
    if (!appOptions->developer()->enableEmbeddedOptionsDialog) {
        AppOptionsDialog::showStandaloneDialog(option, this);
        return;
    }
    if (!m_modalHost) {
        m_modalHost = new EmbeddedModalHost(this);
        m_modalHost->setGeometry(rect());
        connect(m_modalHost, &EmbeddedModalHost::closed, this,
                &MainWindow::restoreBackgroundInteraction);
    }
    if (!m_appOptionsDialog)
        m_appOptionsDialog = new AppOptionsDialog(this);
    m_focusBeforeModal = qApp->focusWidget();
    m_appOptionsDialog->selectOption(option);
    // Unregister Direct Manipulation while the modal is open: DManip hijacks
    // WM_MOUSEWHEEL on the main window after the first wheel event (converting
    // it into a pan gesture for the editor), so the settings pages would never
    // receive QWheelEvent. Re-registered in restoreBackgroundInteraction().
#if defined(WITH_DIRECT_MANIPULATION)
    unregisterDirectManipulation();
#endif
    m_modalHost->open(m_appOptionsDialog, QSize(900, 600));
    // Suspend AFTER opening so that the panel (a descendant of the host) is
    // excluded from the isAncestorOf()-based filtering.
    suspendBackgroundInteraction();
    // Defer focus transfer to the next event-loop iteration: the menu popup
    // restores focus to the pre-menu widget after the triggering action runs.
    // On Windows the wheel event follows keyboard focus (delivered to the
    // focus widget instead of the widget under the cursor), so without this
    // the panel would never receive wheel events.
    QTimer::singleShot(0, m_appOptionsDialog, [this] { m_appOptionsDialog->setFocus(); });
}

void MainWindow::closeAppOptions() {
    if (m_modalHost)
        m_modalHost->closePanel();
}

void MainWindow::suspendBackgroundInteraction() {
    const auto insideModal = [this](const QObject *object) {
        for (const QObject *p = object; p; p = p->parent())
            if (p == m_modalHost)
                return true;
        return false;
    };
    for (auto *action : findChildren<QAction *>()) {
        if (insideModal(action))
            continue;
        if (!action->isEnabled())
            continue;
        m_suspendedActions.append(action);
        action->setEnabled(false);
    }
    for (auto *shortcut : findChildren<QShortcut *>()) {
        if (insideModal(shortcut))
            continue;
        if (!shortcut->isEnabled())
            continue;
        m_suspendedShortcuts.append(shortcut);
        shortcut->setEnabled(false);
    }
}

void MainWindow::restoreBackgroundInteraction() {
    // Skip actions that were destroyed while the modal was open (see
    // m_suspendedActions); QPointer turns them into null automatically.
    for (const auto &action : std::as_const(m_suspendedActions)) {
        if (action)
            action->setEnabled(true);
    }
    m_suspendedActions.clear();
    for (const auto shortcut : std::as_const(m_suspendedShortcuts))
        shortcut->setEnabled(true);
    m_suspendedShortcuts.clear();

    // Give focus back to the widget that had it before the modal opened,
    // so it does not linger on the (now hidden) settings panel.
    if (m_focusBeforeModal && m_focusBeforeModal->isVisible())
        m_focusBeforeModal->setFocus();
    m_focusBeforeModal.clear();

    // Re-register Direct Manipulation, mirroring the unregister in
    // openAppOptions(); wheel gesture support must be restored for the editor.
#if defined(WITH_DIRECT_MANIPULATION)
    registerDirectManipulation();
#endif
}

QWidget *MainWindow::documentWorkflowParentWidget() {
    return this;
}

SaveDecision MainWindow::askDocumentSaveDecision() {
    SaveDecision decision = SaveDecision::Cancel;
    Dialog dialog(this);
    dialog.setWindowTitle(tr("Warning"));
    dialog.setTitle(tr("Do you want to save changes?"));
    dialog.setModal(true);

    auto btnSave = new AccentButton(tr("Save"));
    connect(btnSave, &AccentButton::clicked, &dialog, [&] {
        decision = SaveDecision::Save;
        dialog.accept();
    });
    dialog.setPositiveButton(btnSave);

    auto btnDoNotSave = new Button(tr("Don't save"));
    connect(btnDoNotSave, &Button::clicked, &dialog, [&] {
        decision = SaveDecision::Discard;
        dialog.accept();
    });
    dialog.setNegativeButton(btnDoNotSave);

    auto btnCancel = new Button(tr("Cancel"));
    connect(btnCancel, &Button::clicked, &dialog, [&] {
        decision = SaveDecision::Cancel;
        dialog.reject();
    });
    dialog.setNeutralButton(btnCancel);
    dialog.exec();
    return decision;
}

QString MainWindow::chooseDocumentSavePath(const QString &suggestedPath) {
    return QFileDialog::getSaveFileName(this, tr("Save project"), suggestedPath,
                                        tr("DiffScope Project File (*.dspx)"));
}

bool MainWindow::confirmOpenWithoutPackageMetadata() {
    MessageDialog dialog(tr("Package scan failed"),
                         tr("Singer package metadata is not available. Open the project anyway?"));
    dialog.setTitle(tr("Package scan failed"));
    dialog.addAccentButton(tr("Open Anyway"), 1);
    dialog.addButton(tr("Cancel"), 0);
    return dialog.exec() == 1;
}

void MainWindow::showDocumentWorkflowError(const ProjectOperationError &error) {
    MessageDialog dialog;
    dialog.setWindowTitle(tr("Error"));
    dialog.setTitle(error.title);
    dialog.setMessage(error.message);
    dialog.addAccentButton(tr("OK"), 1);
    dialog.exec();
}

void MainWindow::showDocumentWorkflowBusy() {
    Toast::show(tr("Another document operation is already in progress"));
}

EditorViewState MainWindow::captureEditorViewState() const {
    const auto *clipEditor = m_bottomPanelView->clipEditorView();
    return {
        .trackPanel = m_trackEditorView->viewState(),
        .layout =
            {
                     .trackPanelVisible = !appStatus->trackPanelCollapsed,
                     .bottomPanelVisible = !appStatus->bottomPanelCollapsed,
                     .pianoRollVisible = clipEditor->regionVisible(EditorViewGlobal::Region::PianoRoll),
                     .parametersVisible =
                    clipEditor->regionVisible(EditorViewGlobal::Region::Parameters),
                     .bottomPanelPageId = m_bottomPanelView->currentPageId(),
                     .activeRegion = editorViewController->activeRegion(),
                     .focusedRegion = focusedEditorRegion(),
                     },
        .pianoRoll = clipEditor->viewState(),
        .parameters = clipEditor->parameterViewState(),
    };
}

bool MainWindow::restoreEditorViewState(const EditorViewState &state) {
    const auto finite = [](const double value) { return std::isfinite(value); };
    const auto &layout = state.layout;
    const bool focusedRegionVisible =
        layout.focusedRegion == EditorViewGlobal::Region::None ||
        (layout.focusedRegion == EditorViewGlobal::Region::TrackPanel &&
         layout.trackPanelVisible) ||
        (layout.bottomPanelVisible && layout.bottomPanelPageId == QStringLiteral("ClipEditor") &&
         ((layout.focusedRegion == EditorViewGlobal::Region::PianoRoll &&
           layout.pianoRollVisible) ||
          (layout.focusedRegion == EditorViewGlobal::Region::Parameters &&
           layout.parametersVisible)));
    if ((!state.layout.trackPanelVisible && !state.layout.bottomPanelVisible) ||
        (!state.layout.pianoRollVisible && !state.layout.parametersVisible) ||
        !focusedRegionVisible ||
        !m_bottomPanelView->hasPage(state.layout.bottomPanelPageId) ||
        !m_bottomPanelView->clipEditorView()->supportsEditMode(state.pianoRoll.editMode) ||
        !finite(state.trackPanel.centerTick) || !finite(state.trackPanel.centerTrackIndex) ||
        !finite(state.trackPanel.horizontalScale) || !finite(state.trackPanel.verticalScale) ||
        state.trackPanel.horizontalScale <= 0 || state.trackPanel.verticalScale <= 0 ||
        !finite(state.pianoRoll.centerTick) || !finite(state.pianoRoll.centerKeyIndex) ||
        !finite(state.pianoRoll.horizontalScale) || !finite(state.pianoRoll.verticalScale) ||
        state.pianoRoll.horizontalScale <= 0 || state.pianoRoll.verticalScale <= 0 ||
        state.parameters.foreground <= ParamInfo::Pitch ||
        state.parameters.foreground >= ParamInfo::Unknown ||
        state.parameters.background < ParamInfo::Expressiveness ||
        state.parameters.background == ParamInfo::SpeakerMix ||
        state.parameters.background > ParamInfo::Unknown ||
        state.parameters.editMode < EditorViewGlobal::ParameterEditMode::Draw ||
        state.parameters.editMode > EditorViewGlobal::ParameterEditMode::Anchor ||
        !finite(state.parameters.centerRatio) || state.parameters.centerRatio < 0.0 ||
        state.parameters.centerRatio > 1.0 || !finite(state.parameters.verticalScale) ||
        state.parameters.verticalScale < 1.0) {
        return false;
    }

    m_bottomPanelView->setCurrentPageId(state.layout.bottomPanelPageId);
    setEditorPanelVisibility(state.layout.trackPanelVisible, state.layout.bottomPanelVisible);
    m_trackEditorView->setViewScale(state.trackPanel.horizontalScale,
                                    state.trackPanel.verticalScale);
    m_trackEditorView->centerAt(state.trackPanel.centerTick, state.trackPanel.centerTrackIndex);
    const auto clipEditor = m_bottomPanelView->clipEditorView();
    if (!clipEditor->setRegionVisibility(state.layout.pianoRollVisible,
                                         state.layout.parametersVisible)) {
        return false;
    }
    clipEditor->setViewScale(state.pianoRoll.horizontalScale, state.pianoRoll.verticalScale);
    clipEditor->centerAt(state.pianoRoll.centerTick, state.pianoRoll.centerKeyIndex);
    clipEditor->setEditMode(state.pianoRoll.editMode);
    if (clipEditor->hasActiveSingingClip()) {
        if (!clipEditor->setParameterForeground(state.parameters.foreground) ||
            !clipEditor->setParameterBackground(state.parameters.background) ||
            !clipEditor->setParameterEditMode(state.parameters.editMode) ||
            !clipEditor->setParameterValueViewport(state.parameters.centerRatio,
                                                   state.parameters.verticalScale)) {
            return false;
        }
    }
    editorViewController->setActiveRegion(state.layout.activeRegion);
    if (state.layout.focusedRegion != EditorViewGlobal::Region::None &&
        !focusEditorRegion(state.layout.focusedRegion)) {
        return false;
    }
    return true;
}

bool MainWindow::centerTrackPanelAt(const double tick, const double trackIndex) {
    return m_trackEditorView->centerAt(tick, trackIndex);
}

bool MainWindow::setTrackPanelScale(const double horizontalScale, const double verticalScale) {
    return m_trackEditorView->setViewScale(horizontalScale, verticalScale);
}

bool MainWindow::setTrackPanelViewport(const TrackPanelViewState &state) {
    return m_trackEditorView->setViewport(state);
}

bool MainWindow::setEditorPanelVisibility(const bool trackPanelVisible,
                                          const bool bottomPanelVisible) {
    if (!trackPanelVisible && !bottomPanelVisible)
        return false;

    if (m_bottomPanelDetached) {
        m_trackEditorView->setVisible(trackPanelVisible);
        m_bottomPanelView->setVisible(bottomPanelVisible);
    } else {
        // Detached mode controls widget visibility directly. Re-enable both children before
        // applying docked collapse state through splitter sizes.
        m_trackEditorView->setVisible(true);
        m_bottomPanelView->setVisible(true);
        const auto currentSizes = m_splitter->sizes();
        const bool bothCurrentlyVisible =
            currentSizes.size() >= 2 && currentSizes.at(0) > 0 && currentSizes.at(1) > 0;
        if (bothCurrentlyVisible && (!trackPanelVisible || !bottomPanelVisible))
            m_splitterState = m_splitter->saveState();

        if (trackPanelVisible && bottomPanelVisible) {
            if (!bothCurrentlyVisible &&
                (m_splitterState.isEmpty() || !m_splitter->restoreState(m_splitterState))) {
                m_splitter->setSizes({1, 1});
            }
        } else if (trackPanelVisible) {
            m_splitter->setSizes({1, 0});
        } else {
            m_splitter->setSizes({0, 1});
        }
    }

    updatePanelVisibilityState(trackPanelVisible, bottomPanelVisible);
    return true;
}

bool MainWindow::showBottomPanelPage(const QString &pageId) {
    if (!m_bottomPanelView->hasPage(pageId))
        return false;
    if (appStatus->bottomPanelCollapsed) {
        const bool trackPanelVisible = !appStatus->trackPanelCollapsed;
        if (!setEditorPanelVisibility(trackPanelVisible, true))
            return false;
    }
    return m_bottomPanelView->setCurrentPageId(pageId);
}

bool MainWindow::showEditorRegion(const EditorViewGlobal::Region region) {
    if (editorFocusControlBlocked())
        return false;
    if (region == EditorViewGlobal::Region::TrackPanel) {
        if (appStatus->trackPanelCollapsed &&
            !setEditorPanelVisibility(true, !appStatus->bottomPanelCollapsed)) {
            return false;
        }
        editorViewController->setActiveRegion(region);
        return true;
    }
    if (region != EditorViewGlobal::Region::PianoRoll &&
        region != EditorViewGlobal::Region::Parameters) {
        return false;
    }
    if (!showBottomPanelPage(QStringLiteral("ClipEditor")))
        return false;
    auto *clipEditor = m_bottomPanelView->clipEditorView();
    if (!clipEditor->showRegion(region))
        return false;
    editorViewController->setActiveRegion(region);
    return true;
}

bool MainWindow::focusEditorRegion(const EditorViewGlobal::Region region) {
    if (!showEditorRegion(region))
        return false;
    if (region == EditorViewGlobal::Region::TrackPanel) {
        return m_trackEditorView->focusEditor();
    }
    auto *clipEditor = m_bottomPanelView->clipEditorView();
    return clipEditor->focusRegion(region);
}

bool MainWindow::centerPianoRollAt(const double tick, const double keyIndex) {
    return m_bottomPanelView->clipEditorView()->centerAt(tick, keyIndex);
}

bool MainWindow::setPianoRollScale(const double horizontalScale, const double verticalScale) {
    return m_bottomPanelView->clipEditorView()->setViewScale(horizontalScale, verticalScale);
}

bool MainWindow::setClipEditorTimeViewport(const double centerTick, const double horizontalScale) {
    return m_bottomPanelView->clipEditorView()->setTimeViewport(centerTick, horizontalScale);
}

bool MainWindow::setPianoRollPitchViewport(const double centerKeyIndex,
                                           const double verticalScale) {
    return m_bottomPanelView->clipEditorView()->setPitchViewport(centerKeyIndex, verticalScale);
}

bool MainWindow::setPianoRollEditMode(const EditorViewGlobal::PianoRollEditMode mode) {
    return m_bottomPanelView->clipEditorView()->setEditMode(mode);
}

bool MainWindow::setParameterForeground(const ParamInfo::Name name) {
    if (editorFocusControlBlocked())
        return false;
    auto *clipEditor = m_bottomPanelView->clipEditorView();
    if (!showEditorRegion(EditorViewGlobal::Region::Parameters) ||
        !clipEditor->setParameterForeground(name)) {
        return false;
    }
    focusEditorRegion(EditorViewGlobal::Region::Parameters);
    return true;
}

bool MainWindow::setParameterBackground(const ParamInfo::Name name) {
    if (editorFocusControlBlocked())
        return false;
    auto *clipEditor = m_bottomPanelView->clipEditorView();
    if (!showEditorRegion(EditorViewGlobal::Region::Parameters) ||
        !clipEditor->setParameterBackground(name)) {
        return false;
    }
    focusEditorRegion(EditorViewGlobal::Region::Parameters);
    return true;
}

bool MainWindow::swapParameters() {
    if (editorFocusControlBlocked())
        return false;
    auto *clipEditor = m_bottomPanelView->clipEditorView();
    if (!showEditorRegion(EditorViewGlobal::Region::Parameters) || !clipEditor->swapParameters())
        return false;
    focusEditorRegion(EditorViewGlobal::Region::Parameters);
    return true;
}

bool MainWindow::setParameterEditMode(const EditorViewGlobal::ParameterEditMode mode) {
    if (editorFocusControlBlocked())
        return false;
    auto *clipEditor = m_bottomPanelView->clipEditorView();
    if (!showEditorRegion(EditorViewGlobal::Region::Parameters) ||
        !clipEditor->setParameterEditMode(mode)) {
        return false;
    }
    focusEditorRegion(EditorViewGlobal::Region::Parameters);
    return true;
}

bool MainWindow::setParameterValueViewport(const double centerRatio, const double verticalScale) {
    if (editorFocusControlBlocked())
        return false;
    auto *clipEditor = m_bottomPanelView->clipEditorView();
    if (!showEditorRegion(EditorViewGlobal::Region::Parameters) ||
        !clipEditor->setParameterValueViewport(centerRatio, verticalScale)) {
        return false;
    }
    focusEditorRegion(EditorViewGlobal::Region::Parameters);
    return true;
}

void MainWindow::refreshActiveClipTrackPresentation() {
    m_bottomPanelView->clipEditorView()->refreshActiveClipTrackPresentation();
}

void MainWindow::previewActiveClipTrackColor(const int colorIndex) {
    m_bottomPanelView->clipEditorView()->previewActiveClipTrackColor(colorIndex);
}

HistoryFocusVisibility MainWindow::focusVisibility(const HistoryFocus &focus) const {
    if (focus.kind == HistoryFocusKind::TrackClips) {
        if (appStatus->trackPanelCollapsed)
            return HistoryFocusVisibility::ContextSwitchRequired;
        return m_trackEditorView->focusVisibility(focus);
    }
    if (focus.kind == HistoryFocusKind::PianoRollNotes) {
        if (appStatus->bottomPanelCollapsed ||
            m_bottomPanelView->currentPageId() != QStringLiteral("ClipEditor") ||
            !m_bottomPanelView->clipEditorView()->regionVisible(
                EditorViewGlobal::Region::PianoRoll) ||
            appStatus->activeClipId != focus.containerId) {
            return HistoryFocusVisibility::ContextSwitchRequired;
        }
        return m_bottomPanelView->clipEditorView()->focusVisibility(focus);
    }
    return HistoryFocusVisibility::Unavailable;
}

EditorViewGlobal::Region MainWindow::focusedEditorRegion() const {
    auto *focused = QApplication::focusWidget();
    if (!focused)
        return EditorViewGlobal::Region::None;
    if (focused == m_trackEditorView || m_trackEditorView->isAncestorOf(focused))
        return EditorViewGlobal::Region::TrackPanel;
    return m_bottomPanelView->clipEditorView()->focusedRegion();
}

bool MainWindow::editorFocusControlBlocked() const {
    return (m_modalHost && m_modalHost->isOpen()) || QApplication::activeModalWidget();
}

bool MainWindow::revealFocus(const HistoryFocus &focus) {
    const auto visibility = focusVisibility(focus);
    return navigateToFocus(focus, visibility == HistoryFocusVisibility::ScrollRequired);
}

bool MainWindow::navigateToFocus(const HistoryFocus &focus, const bool animated) {
    if (focus.kind == HistoryFocusKind::TrackClips) {
        if (!showEditorRegion(EditorViewGlobal::Region::TrackPanel) ||
            !m_trackEditorView->revealFocus(focus, animated)) {
            return false;
        }
        focusEditorRegion(EditorViewGlobal::Region::TrackPanel);
        return true;
    }
    if (focus.kind == HistoryFocusKind::PianoRollNotes) {
        if (!appModel->findClipById(focus.containerId))
            return false;
        trackController->setActiveClip(focus.containerId);
        if (!showEditorRegion(EditorViewGlobal::Region::PianoRoll) ||
            !m_bottomPanelView->clipEditorView()->revealFocus(focus, animated)) {
            return false;
        }
        focusEditorRegion(EditorViewGlobal::Region::PianoRoll);
        return true;
    }
    return false;
}

bool MainWindow::finalizeFocus(const HistoryFocus &focus) {
    return navigateToFocus(focus, false);
}

void MainWindow::clearFocusPreview() {
}

void MainWindow::onAllDone() {
    if (m_isCloseRequested) {
        m_isAllDone = true;
        close();
    }
}

void MainWindow::onSplitterMoved(int pos, int index) {
    const auto sizes = m_splitter->sizes();
    if (sizes.size() < 2)
        return;

    if (sizes.at(0) == 0) {
        updatePanelVisibilityState(false, true);
    } else if (sizes.at(1) == 0) {
        updatePanelVisibilityState(true, false);
    } else {
        m_splitterState = m_splitter->saveState();
        updatePanelVisibilityState(true, true);
    }
}

void MainWindow::updatePanelVisibilityState(const bool trackPanelVisible,
                                            const bool bottomPanelVisible) {
    appStatus->trackPanelCollapsed = !trackPanelVisible;
    appStatus->bottomPanelCollapsed = !bottomPanelVisible;
    editorViewController->syncPanelVisibility(trackPanelVisible, bottomPanelVisible,
                                              m_bottomPanelView->panelType());
}

void MainWindow::detachBottomPanel() {
    if (m_bottomPanelDetached)
        return;

    m_bottomPanelDetached = true;
    m_detachSplitterState = m_splitter->saveState();

    m_bottomPanelView->setParent(nullptr, Qt::Window);
    m_bottomPanelView->titleBar()->setDetached(true, m_useNativeFrame);

    if (!m_useNativeFrame) {
        m_detachedAgent = new QWK::WidgetWindowAgent(m_bottomPanelView);
        m_detachedAgent->setup(m_bottomPanelView);
        m_detachedAgent->setTitleBar(m_bottomPanelView->titleBar());
        auto *titleBar = m_bottomPanelView->titleBar();
        m_detachedAgent->setSystemButton(QWK::WindowAgentBase::Minimize,
                                         titleBar->minimizeButton());
        m_detachedAgent->setSystemButton(QWK::WindowAgentBase::Maximize,
                                         titleBar->maximizeButton());
        m_detachedAgent->setSystemButton(QWK::WindowAgentBase::Close, titleBar->closeButton());
        m_detachedAgent->setHitTestVisible(static_cast<QWidget *>(titleBar->tabBar()));
        m_detachedAgent->setHitTestVisible(static_cast<QWidget *>(titleBar->toolBar()));

        connect(titleBar->minimizeButton(), &Button::clicked, m_bottomPanelView,
                &QWidget::showMinimized);
        connect(titleBar->maximizeButton(), &Button::clicked, m_bottomPanelView, [this] {
            if (m_bottomPanelView->isMaximized())
                m_bottomPanelView->showNormal();
            else
                m_bottomPanelView->showMaximized();
        });
        connect(titleBar->closeButton(), &Button::clicked, this, &MainWindow::attachBottomPanel);
    }

    ThemeManager::instance()->addStyleRoot(m_bottomPanelView);

    m_bottomPanelView->setMinimumWidth(960);

    if (m_detachedWindowGeometry.isValid()) {
        m_bottomPanelView->setGeometry(m_detachedWindowGeometry);
    } else {
        int panelHeight = m_bottomPanelView->height();
        int panelWidth = 960;
        m_bottomPanelView->resize(panelWidth, panelHeight);

        auto scr = QApplication::screenAt(QCursor::pos());
        if (!scr)
            scr = QApplication::primaryScreen();
        if (scr) {
            auto availableRect = scr->availableGeometry();
            int x = availableRect.x() + (availableRect.width() - panelWidth) / 2;
            int y = availableRect.y() + (availableRect.height() - panelHeight) / 2;
            m_bottomPanelView->move(x, y);
        }
    }
    m_bottomPanelView->show();

    m_bottomPanelView->installEventFilter(this);

#if defined(WITH_DIRECT_MANIPULATION)
    if (appOptions->appearance()->enableDirectManipulation) {
        QWDMH::DirectManipulationSystem::registerWindow(m_bottomPanelView->windowHandle());
    }
#endif
}

void MainWindow::attachBottomPanel() {
    if (!m_bottomPanelDetached)
        return;

    m_bottomPanelDetached = false;
    m_detachedWindowGeometry = m_bottomPanelView->geometry();

    m_bottomPanelView->removeEventFilter(this);

#if defined(WITH_DIRECT_MANIPULATION)
    if (appOptions->appearance()->enableDirectManipulation) {
        QWDMH::DirectManipulationSystem::unregisterWindow(m_bottomPanelView->windowHandle());
    }
#endif

    if (m_detachedAgent) {
        delete m_detachedAgent;
        m_detachedAgent = nullptr;
    }

    m_bottomPanelView->hide();
    m_bottomPanelView->titleBar()->setDetached(false, m_useNativeFrame);
    m_bottomPanelView->setParent(m_splitter, Qt::Widget);
    m_bottomPanelView->setMinimumWidth(0);

    m_splitter->insertWidget(1, m_bottomPanelView);
    ThemeManager::instance()->removeStyleRoot(m_bottomPanelView);
    m_splitter->restoreState(m_detachSplitterState);
    m_bottomPanelView->show();
    setEditorPanelVisibility(!appStatus->trackPanelCollapsed, !appStatus->bottomPanelCollapsed);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_bottomPanelView && event->type() == QEvent::WindowActivate) {
        editorViewController->activatePanelContext(m_bottomPanelView->panelType());
        return false;
    }
    if (watched == m_bottomPanelView && event->type() == QEvent::Close) {
        event->ignore();
        attachBottomPanel();
        return true;
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_bottomPanelDetached)
        attachBottomPanel();

    if (!m_documentCloseApproved) {
        event->ignore();
        documentWorkflowController->requestTermination(TerminationMode::Exit);
        return;
    }
    if (m_isAllDone) {
        if (m_waitDoneDialog)
            m_waitDoneDialog->forceClose();
        if (m_restartRequested)
            restartApp();
        QMainWindow::closeEvent(event);
    } else if (m_isCloseRequested) {
        qDebug() << "Waiting for all tasks done...";
        event->ignore();
    } else {
        m_isCloseRequested = true;
        qDebug() << "Terminating background tasks...";
        m_waitDoneDialog = new TaskDialog(nullptr, false, false, this);
        m_waitDoneDialog->setTitle(tr("%1 is exiting...").arg(qApp->applicationDisplayName()));
        m_waitDoneDialog->setMessage(tr("Terminating background tasks..."));

        m_waitDoneDialogDelayTimer.setSingleShot(true);
        m_waitDoneDialogDelayTimer.setInterval(500);
        connect(&m_waitDoneDialogDelayTimer, &QTimer::timeout, this, [this] {
            if (m_waitDoneDialog)
                m_waitDoneDialog->show();
        });
        m_waitDoneDialogDelayTimer.start();

        taskManager->terminateAllTasks();
        auto thread = new QThread;
        taskManager->moveToThread(thread);
        connect(thread, &QThread::started, taskManager, &TaskManager::wait);
        thread->start();
        // Keep the window alive until background tasks have been terminated
        // (TaskManager::allDone -> onAllDone -> close()). Accepting here would
        // close the window and quit the event loop before onAllDone runs, so
        // restartApp() (the "Restart Now" path) would never be reached.
        event->ignore();
    }
}

#if defined(WITH_DIRECT_MANIPULATION)
void MainWindow::registerDirectManipulation() {
    if (!m_isDirectManipulationRegistered) {
        QWDMH::DirectManipulationSystem::registerWindow(windowHandle());
        m_isDirectManipulationRegistered = true;
    }
}

void MainWindow::unregisterDirectManipulation() {
    if (m_isDirectManipulationRegistered) {
        QWDMH::DirectManipulationSystem::unregisterWindow(windowHandle());
        m_isDirectManipulationRegistered = false;
    }
}
#endif

void MainWindow::updateShutdownBlockReason() {
#ifdef Q_OS_WIN
    const bool onSavePoint = historyManager->isOnSavePoint();
    HWND hwnd = reinterpret_cast<HWND>(this->winId());
    if (onSavePoint) {
        ShutdownBlockReasonDestroy(hwnd);
    } else {
        const QString reason = tr("You have unsaved changes, please save first");
        ShutdownBlockReasonCreate(hwnd, reinterpret_cast<LPCWSTR>(reason.utf16()));
    }
#endif
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_QUERYENDSESSION) {
            *result = historyManager->isOnSavePoint() ? TRUE : FALSE;
            if (*result == FALSE)
                updateShutdownBlockReason();
            close();
            return true;
        } else if (msg->message == WM_SETTINGCHANGE) {
            const auto changedSetting = reinterpret_cast<LPCWSTR>(msg->lParam);
            if (changedSetting && lstrcmpW(changedSetting, L"ImmersiveColorSet") == 0) {
                qDebug() << "WM_SETTINGCHANGE triggered: ImmersiveColorSet";
                ThemeManager::instance()->onSystemThemeColorChanged();
            }
        }
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::emulateLeaveEvent(QWidget *widget) {
    Q_ASSERT(widget);
    QTimer::singleShot(0, widget, [widget]() {
        const QScreen *screen = widget->screen();
        const QPoint globalPos = QCursor::pos(screen);
        if (!QRect(widget->mapToGlobal(QPoint{0, 0}), widget->size()).contains(globalPos)) {
            QCoreApplication::postEvent(widget, new QEvent(QEvent::Leave));
            if (widget->testAttribute(Qt::WA_Hover)) {
                const QPoint localPos = widget->mapFromGlobal(globalPos);
                const QPoint scenePos = widget->window()->mapFromGlobal(globalPos);
                static constexpr const auto oldPos = QPoint{};
                const Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
                const auto event =
                    new QHoverEvent(QEvent::HoverLeave, scenePos, globalPos, oldPos, modifiers);
                Q_UNUSED(localPos);
                QCoreApplication::postEvent(widget, event);
            }
        }
    });
}

void MainWindow::restartApp() {
    qApp->setProperty("restart", true);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        for (const QUrl &url : event->mimeData()->urls()) {
            if (!url.isLocalFile())
                continue;
            const auto kind = ExternalFileClassifier::classify(url.toLocalFile()).kind;
            if (kind != ExternalFileKind::Unsupported) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void MainWindow::dropEvent(QDropEvent *event) {
    QStringList projectPaths;
    QStringList importPaths;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (!url.isLocalFile())
            continue;
        const auto path = url.toLocalFile();
        const auto kind = ExternalFileClassifier::classify(path).kind;
        if (kind == ExternalFileKind::Project)
            projectPaths.append(path);
        else if (kind != ExternalFileKind::Unsupported)
            importPaths.append(path);
    }

    if (projectPaths.size() == 1 && importPaths.isEmpty()) {
        documentWorkflowController->requestOpen(projectPaths.first());
        return;
    }
    // Mixing a project with other files rejects the whole batch inside the
    // import controller; canvas-outside drops import at the project end.
    importPaths.append(projectPaths);
    if (!importPaths.isEmpty())
        documentImportController->requestImport(importPaths);
}
