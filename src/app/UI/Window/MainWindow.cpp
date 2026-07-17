//
// Created by fluty on 2024/1/31.
//

#include "MainWindow.h"

#ifdef Q_OS_WIN
#  include <Windows.h>
#  include <WinUser.h>
#endif

#include "Controller/AppController.h"
#include "Controller/AudioDecodingController.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Controller/TrackController.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/History/HistoryManager.h"
#include "Modules/PackageManager/PackageManager.h"
#include "Modules/Task/TaskManager.h"
#include "UI/Controls/AccentButton.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/SilentSplitter.h"
#include "UI/Controls/Toast.h"
#include "UI/Dialogs/Base/MessageDialog.h"
#include "UI/Dialogs/Base/TaskDialog.h"
#include "UI/Dialogs/ResourceCheck/AudioResourcePage.h"
#include "UI/Dialogs/ResourceCheck/ResourceCheckDialog.h"
#include "UI/Views/BottomPanelView.h"
#include "UI/Views/ClipEditor/ClipEditorView.h"
#include "UI/Views/Common/TabPanelTitleBar.h"
#include "UI/Views/MainTitleBar/TitleBarComboBox.h"
#include "UI/Views/MainTitleBar/FilePopupWidget.h"
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
#include "Utils/WindowFrameUtils.h"

#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMimeData>
#include <QProcess>
#include <QSplitter>
#include <QWKWidgets/widgetwindowagent.h>

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

        // Connect file popup actions
        auto *filePopup = m_titleBar->titleComboBox()->popupWidget();
        connect(filePopup, &FilePopupWidget::newProjectClicked, m_mainMenu->actionNew(),
                &QAction::trigger);
        connect(filePopup, &FilePopupWidget::openProjectClicked, m_mainMenu->actionOpen(),
                &QAction::trigger);
        connect(filePopup, &FilePopupWidget::openRecentProject, m_mainMenu,
                &MainMenuView::openRecentProject);
    }
    installEventFilter(m_titleBar);

    QString styleSheet;
    QStringList qssFileList = {
        ":theme/lite-dark/base.qss",        ":theme/lite-dark/controls.qss",
        ":theme/lite-dark/title-bar.qss",   ":theme/lite-dark/track-editor.qss",
        ":theme/lite-dark/clip-editor.qss",
    };

    for (const auto &file : qssFileList) {
        if (auto qssFile = QFile(file); qssFile.open(QIODevice::ReadOnly)) {
            styleSheet += qssFile.readAll();
            qssFile.close();
        } else {
            qCritical() << "Failed to open qss:" << file;
        }
    }
    setStyleSheet(styleSheet);

    Dialog::setGlobalContext(this);
    Toast::setGlobalContext(this);
    appController->setMainWindow(this);
    documentWorkflowController->setUi(this);
    connect(documentWorkflowController, &DocumentWorkflowController::documentIdentityChanged, this,
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

    auto scr = QApplication::screenAt(QCursor::pos());
    if (!scr) {
        scr = QApplication::primaryScreen();
    }
    if (scr) {
        auto availableRect = scr->availableGeometry();
        if (availableRect.width() > 1536 && availableRect.height() > 816)
            resize(1536, 816);
        else
            resize(1366, 768);
    } else {
        // Fallback if no screen is available
        resize(1366, 768);
    }

    ThemeManager::instance()->addWindow(this);
#if defined(WITH_DIRECT_MANIPULATION)
    connect(appOptions, &AppOptions::optionsChanged, [&](AppOptionsGlobal::Option option) {
        if (option == AppOptionsGlobal::Option::Appearance) {
            if (appOptions->appearance()->enableDirectManipulation) {
                registerDirectManipulation();
            } else {
                unregisterDirectManipulation();
            }
        }
    });
#endif
    documentWorkflowController->initializeNewDocument();
}

MainWindow::~MainWindow() {
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
        auto displayName = projectPath.isEmpty() ? QFileInfo(projectName).completeBaseName()
                                                 : QFileInfo(projectPath).completeBaseName();
        auto indicator = saved ? "" : "● ";
        setWindowTitle(indicator + displayName);
    }
}

void MainWindow::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        updateWindowTitle();
}

void MainWindow::quit() {
    documentWorkflowController->requestTermination(TerminationMode::Exit);
}

void MainWindow::restart() {
    documentWorkflowController->requestTermination(TerminationMode::Restart);
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

void MainWindow::setTrackAndClipPanelCollapsed(bool trackCollapsed, bool clipCollapsed) {
    if (trackCollapsed && clipCollapsed) {
        qFatal() << "Cannot set track and clip panel collapsed";
        return;
    }

    if (trackCollapsed) {
        m_splitterState = m_splitter->saveState();
        m_splitter->setSizes({0, 100});
        appStatus->trackPanelCollapsed = true;
        appStatus->clipPanelCollapsed = false;
    } else if (clipCollapsed) {
        m_splitterState = m_splitter->saveState();
        m_splitter->setSizes({100, 0});
        appStatus->trackPanelCollapsed = false;
        appStatus->clipPanelCollapsed = true;
    } else {
        // TODO: 恢复到折叠前的大小
        m_splitter->setSizes({240, 100}); // 临时解决方案
        appStatus->trackPanelCollapsed = false;
        appStatus->clipPanelCollapsed = false;
    }
}

void MainWindow::onAllDone() {
    if (m_isCloseRequested) {
        m_isAllDone = true;
        close();
    }
}

void MainWindow::onSplitterMoved(int pos, int index) const {
    qDebug() << "MainWindow::onSplitterMoved"
             << "size 0:" << m_splitter->sizes().at(0) << "size 1:" << m_splitter->sizes().at(1);
    if (m_splitter->sizes().at(0) == 0) {
        appStatus->trackPanelCollapsed = true;
        appStatus->clipPanelCollapsed = false;
    } else if (m_splitter->sizes().at(1) == 0) {
        appStatus->trackPanelCollapsed = false;
        appStatus->clipPanelCollapsed = true;
    } else {
        appStatus->trackPanelCollapsed = false;
        appStatus->clipPanelCollapsed = false;
    }
}

void MainWindow::detachBottomPanel() {
    if (m_bottomPanelDetached)
        return;

    m_bottomPanelDetached = true;
    m_detachSplitterState = m_splitter->saveState();

    m_splitter->widget(1)->setParent(nullptr);

    m_bottomPanelView->setWindowFlags(Qt::Window);
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

    m_bottomPanelView->setStyleSheet(styleSheet());

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
    m_bottomPanelView->setWindowFlags({});
    m_bottomPanelView->setMinimumWidth(0);

    m_splitter->addWidget(m_bottomPanelView);
    m_splitter->restoreState(m_detachSplitterState);
    m_bottomPanelView->show();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
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
        event->accept();
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

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_QUERYENDSESSION) {
            *result = historyManager->isOnSavePoint() ? TRUE : FALSE;
            close();
            return true;
        } else if (msg->message == WM_SETTINGCHANGE) {
            if (lstrcmpW(reinterpret_cast<LPCWSTR>(msg->lParam), L"ImmersiveColorSet") == 0) {
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
        bool validFile = false;

        for (const QUrl &url : event->mimeData()->urls()) {
            const QFileInfo fileInfo(url.toLocalFile());
            if (fileInfo.suffix().toLower() == "dspx") {
                validFile = true;
                break;
            }
        }

        if (validFile) {
            event->acceptProposedAction();
            return;
        }
    }
    event->ignore();
}

void MainWindow::dropEvent(QDropEvent *event) {
    for (const QUrl &url : event->mimeData()->urls()) {
        const QFileInfo fileInfo(url.toLocalFile());

        if (fileInfo.suffix().toLower() == "dspx") {
            auto openProject = [&] {
                const auto fileName = fileInfo.absoluteFilePath();
                if (fileName.isNull())
                    return;
                documentWorkflowController->requestOpen(fileName);
            };
            openProject();
        }
    }
}
