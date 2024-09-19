//
// Created by fluty on 2024/1/31.
//

#include "MainWindow.h"

#ifdef Q_OS_WIN
#  include <Windows.h>
#  include <WinUser.h>
#endif

#include "Controller/AppController.h"
#include "Controller/TrackController.h"
#include "Controller/ValidationController.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/History/HistoryManager.h"
#include "Modules/Task/TaskManager.h"
#include "UI/Controls/AccentButton.h"
#include "UI/Controls/ProgressIndicator.h"
#include "UI/Controls/Toast.h"
#include "UI/Dialogs/Base/TaskDialog.h"
#include "UI/Views/ClipEditor/ClipEditorView.h"
#include "UI/Views/MainTitleBar/ActionButtonsView.h"
#include "UI/Views/MainTitleBar/MainMenuView.h"
#include "UI/Views/MainTitleBar/MainTitleBar.h"
#include "UI/Views/MainTitleBar/PlaybackView.h"
#include "UI/Views/TrackEditor/TrackEditorView.h"
#include "Utils/WindowFrameUtils.h"

#include <QApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QProcess>
#include <QSplitter>
#include <QStatusBar>
#include <QWKWidgets/widgetwindowagent.h>

MainWindow::MainWindow() {
    setAcceptDrops(true);

    auto useNativeFrame = appOptions->appearance()->useNativeFrame;
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
    installEventFilter(m_titleBar);

    QString qssBase;
    QStringList qssFileList = {
        ":theme/lite-dark/base.qss",        ":theme/lite-dark/controls.qss",
        ":theme/lite-dark/title-bar.qss",   ":theme/lite-dark/track-editor.qss",
        ":theme/lite-dark/clip-editor.qss",
    };

    for (const auto &file : qssFileList) {
        if (auto qssFile = QFile(file); qssFile.open(QIODevice::ReadOnly)) {
            qssBase += qssFile.readAll();
            qssFile.close();
        } else {
            qCritical() << "Failed to open qss:" << file;
        }
    }

    if (QSysInfo::productType() == "windows") {
        if (QSysInfo::productVersion() == "11")
            setStyleSheet(QString("QMainWindow { background: transparent }") + qssBase);
        else
            setStyleSheet(QString("QMainWindow { background: #232425; }") + qssBase);
    } else
        setStyleSheet(QString("QMainWindow { background: #232425; }") + qssBase);

    Dialog::setGlobalContext(this);
    Toast::setGlobalContext(this);
    appController->setMainWindow(this);

    connect(taskManager, &TaskManager::allDone, this, &MainWindow::onAllDone);
    connect(taskManager, &TaskManager::taskChanged, this, &MainWindow::onTaskChanged);

    connect(m_mainMenu->actionSave(), &QAction::triggered, this, &MainWindow::onSave);
    connect(m_mainMenu->actionSaveAs(), &QAction::triggered, this, &MainWindow::onSaveAs);

    m_trackEditorView = new TrackEditorView;
    m_clipEditView = new ClipEditorView;

    m_splitter = new QSplitter;
    m_splitter->setOrientation(Qt::Vertical);
    m_splitter->addWidget(m_trackEditorView);
    m_splitter->addWidget(m_clipEditView);
    // 让轨道编辑器高度较小，剪辑编辑器高度较大，且在纵向拉伸窗口时优先拉伸钢琴卷帘
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 100);
    m_splitter->setContentsMargins(6, 0, 6, 0);
    connect(m_splitter, &QSplitter::splitterMoved, this, &MainWindow::onSplitterMoved);

    m_lbTaskTitle = new QLabel;
    m_lbTaskTitle->setVisible(false);

    m_statusProgressBar = new ProgressIndicator;
    m_statusProgressBar->setFixedWidth(170);
    m_statusProgressBar->setVisible(false);

    auto statusBar = new QStatusBar(this);
    // statusBar->addWidget(new QLabel("Scroll: Wheel/Shift + Wheel; Zoom: Ctrl + Wheel/Alt + Wheel;
    // "
    //                                 "Double click to create a singing clip"));
    statusBar->addPermanentWidget(m_lbTaskTitle);
    statusBar->addPermanentWidget(m_statusProgressBar);
    statusBar->setFixedHeight(28);
    statusBar->setSizeGripEnabled(false);
    statusBar->setContentsMargins(6, 0, 6, 0);
    statusBar->setVisible(false);
    setStatusBar(statusBar);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(m_titleBar);
    mainLayout->addWidget(m_splitter);
    mainLayout->addWidget(statusBar);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 6);

    auto mainWidget = new QWidget;
    mainWidget->setLayout(mainLayout);

    this->setCentralWidget(mainWidget);

    auto scr = QApplication::screenAt(QCursor::pos());
    auto availableRect = scr->availableGeometry();
    if (availableRect.width() > 1536 || availableRect.height() > 816)
        resize(1536, 816);
    else
        resize(1366, 768);

    WindowFrameUtils::applyFrameEffects(this);
    appController->newProject();
}

void MainWindow::updateWindowTitle() {
    auto projectName = appController->projectName();
    auto saved = historyManager->isOnSavePoint();
    auto appName = qApp->applicationDisplayName();
    if (projectName.isNull() || projectName.isEmpty())
        setWindowTitle(appName);
    else {
        auto projectPath = appController->projectPath();
        if (projectPath.isNull() || projectPath.isEmpty())
            setWindowTitle(projectName + " - " + appName);
        else {
            auto indicator = saved ? "" : "● ";
            setWindowTitle(indicator + projectName + " - " + appName);
        }
    }
}

bool MainWindow::askSaveChanges() {
    auto handled = new bool;
    auto dlg = new Dialog;
    dlg->setWindowTitle(tr("Warning"));
    dlg->setTitle(tr("Do you want to save changes?"));
    dlg->setModal(true);

    auto btnSave = new AccentButton(tr("Save"));
    connect(btnSave, &AccentButton::clicked, this, [=] {
        onSave();
        *handled = true;
        dlg->accept();
    });
    dlg->setPositiveButton(btnSave);

    auto btnDoNotSave = new Button(tr("Don't save"));
    connect(btnDoNotSave, &Button::clicked, this, [=] {
        *handled = true;
        dlg->accept();
    });
    dlg->setNegativeButton(btnDoNotSave);

    auto btnCancel = new Button(tr("Cancel"));
    connect(btnCancel, &Button::clicked, this, [=] {
        *handled = false;
        dlg->accept();
    });
    dlg->setNeutralButton(btnCancel);

    dlg->exec();
    return *handled;
}

void MainWindow::quit() {
    close();
}

void MainWindow::restart() {
    m_restartRequested = true;
    close();
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
        m_splitter->restoreState(m_splitterState);
        appStatus->trackPanelCollapsed = false;
        appStatus->clipPanelCollapsed = false;
    }
}

void MainWindow::onAllDone() {
    qDebug() << "MainWindow::onAllDone";
    if (m_isCloseRequested) {
        m_isAllDone = true;
        close();
    }
}

void MainWindow::onTaskChanged(TaskManager::TaskChangeType type, Task *task, qsizetype index) {
    if (!m_lbTaskTitle || !m_statusProgressBar)
        return;

    auto taskCount = taskManager->tasks().count();
    if (taskCount == 0) {
        m_lbTaskTitle->setText("");
        m_lbTaskTitle->setVisible(false);
        m_statusProgressBar->setVisible(false);
        m_statusProgressBar->setValue(0);
        m_statusProgressBar->setTaskStatus(TaskGlobal::Normal);
    } else {
        if (type == TaskManager::Removed)
            disconnect(task, nullptr, this, nullptr);

        m_lbTaskTitle->setVisible(true);
        m_statusProgressBar->setVisible(true);
        auto firstTask = taskManager->tasks().first();
        connect(firstTask, &Task::statusUpdated, this, &MainWindow::onTaskStatusChanged);
    }
}

void MainWindow::onTaskStatusChanged(const TaskStatus &status) {
    m_lbTaskTitle->setText(status.title);
    m_statusProgressBar->setValue(status.progress);
    m_statusProgressBar->setTaskStatus(status.runningStatus);
    m_statusProgressBar->setIndeterminate(status.isIndetermine);
}

bool MainWindow::onSave() {
    if (appController->projectPath().isEmpty()) {
        onSaveAs();
    } else {
        appController->saveProject(appController->projectPath());
    }
    return true;
}

bool MainWindow::onSaveAs() {
    auto lastDir = appController->projectPath().isEmpty()
                       ? appController->lastProjectFolder() + "/" + appController->projectName()
                       : appController->projectPath();
    auto getFileName = [=] {
        return QFileDialog::getSaveFileName(this, tr("Save project"), lastDir,
                                            tr("DiffScope Project File (*.dspx)"));
    };
    auto fileName = getFileName();
    if (fileName.isNull()) // Canceled
        return false;

    bool saved = appController->saveProject(fileName);
    while (!saved) {
        saved = appController->saveProject(getFileName());
    }
    return true;
}

void MainWindow::onSplitterMoved(int pos, int index) const {
    if (m_splitter->sizes().at(0) == 0) {
        appStatus->trackPanelCollapsed = true;
        appStatus->clipPanelCollapsed = false;
        // qDebug() << "Track editor collapsed";
    } else if (m_splitter->sizes().at(1) == 0) {
        appStatus->trackPanelCollapsed = false;
        appStatus->clipPanelCollapsed = true;
        // qDebug() << "Clip editor collapsed";
    } else {
        appStatus->trackPanelCollapsed = false;
        appStatus->clipPanelCollapsed = false;
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    auto saved = historyManager->isOnSavePoint();
    if (!saved) {
        if (!askSaveChanges()) {
            event->ignore();
            return;
        }
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
        connect(&m_waitDoneDialogDelayTimer, &QTimer::timeout, this, [=] {
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
    // taskManager->wait();
    // QMainWindow::closeEvent(event);
}

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
    auto program = QCoreApplication::applicationFilePath();
    qDebug() << "restart" << program;
    QProcess::startDetached(program);
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
            auto openProject = [=] {
                const auto fileName = fileInfo.absoluteFilePath();
                if (fileName.isNull())
                    return;
                appController->openProject(fileName);
            };
            if (!historyManager->isOnSavePoint()) {
                if (this->askSaveChanges())
                    openProject();
            } else
                openProject();
        }
    }
}