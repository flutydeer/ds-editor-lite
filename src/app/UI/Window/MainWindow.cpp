//
// Created by fluty on 2024/1/31.
//

#include "MainWindow.h"

#ifdef Q_OS_WIN
#  include <Windows.h>
#  include <WinUser.h>
#endif

#include "Controller/AppController.h"
#include "Controller/TracksViewController.h"
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
#include <QWKWidgets/widgetwindowagent.h>

#include <QApplication>
#include <QCloseEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QSplitter>
#include <QStatusBar>

MainWindow::MainWindow() {
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
    auto qssFile = QFile(":theme/lite-dark.qss");
    if (qssFile.open(QIODevice::ReadOnly)) {
        qssBase = qssFile.readAll();
        qssFile.close();
    }
    this->setStyleSheet(QString("QMainWindow { background: #232425; }") + qssBase);
#ifdef Q_OS_WIN
    bool micaOn = true;
    auto version = QSysInfo::productVersion();
    if (micaOn && version == "11") {
        // make window transparent
        this->setStyleSheet(QString("QMainWindow { background: transparent }") + qssBase);
    }
#elif defined(Q_OS_MAC)
    this->setStyleSheet(QString("QMainWindow { background: transparent }") + qssBase);
#endif

    Dialog::setGlobalContext(this);
    Toast::setGlobalContext(this);
    appController->setMainWindow(this);

    connect(taskManager, &TaskManager::allDone, this, &MainWindow::onAllDone);
    connect(taskManager, &TaskManager::taskChanged, this, &MainWindow::onTaskChanged);

    connect(m_mainMenu->actionSave(), &QAction::triggered, this, &MainWindow::onSave);
    connect(m_mainMenu->actionSaveAs(), &QAction::triggered, this, &MainWindow::onSaveAs);

    m_trackEditorView = new TrackEditorView;
    m_clipEditView = new ClipEditorView;

    auto splitter = new QSplitter;
    splitter->setOrientation(Qt::Vertical);
    splitter->addWidget(m_trackEditorView);
    splitter->addWidget(m_clipEditView);
    splitter->setContentsMargins(6, 0, 6, 0);

    ValidationController::instance();
    appController->newProject();

    m_lbTaskTitle = new QLabel;
    m_lbTaskTitle->setVisible(false);

    m_progressBar = new ProgressIndicator;
    m_progressBar->setFixedWidth(170);
    m_progressBar->setVisible(false);

    auto statusBar = new QStatusBar(this);
    statusBar->addWidget(new QLabel("Scroll: Wheel/Shift + Wheel; Zoom: Ctrl + Wheel/Alt + Wheel; "
                                    "Double click to create a singing clip"));
    statusBar->addPermanentWidget(m_lbTaskTitle);
    statusBar->addPermanentWidget(m_progressBar);
    statusBar->setFixedHeight(28);
    statusBar->setSizeGripEnabled(false);
    statusBar->setStyleSheet("QStatusBar::item { border: none } QLabel {color: #A0FFFFFF}");
    setStatusBar(statusBar);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(m_titleBar);
    mainLayout->addWidget(splitter);
    mainLayout->addWidget(statusBar);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins({0, 0, 0, 0});

    auto mainWidget = new QWidget;
    mainWidget->setLayout(mainLayout);

    this->setCentralWidget(mainWidget);
    this->resize(1280, 720);

    WindowFrameUtils::applyFrameEffects(this);
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
void MainWindow::onAllDone() {
    qDebug() << "MainWindow::onAllDone";
    if (m_isCloseRequested) {
        m_isAllDone = true;
        close();
    }
}
void MainWindow::onTaskChanged(TaskManager::TaskChangeType type, Task *task, qsizetype index) {
    auto taskCount = taskManager->tasks().count();
    if (taskCount == 0) {
        m_lbTaskTitle->setText("");
        m_lbTaskTitle->setVisible(false);
        m_progressBar->setVisible(false);
        m_progressBar->setValue(0);
        m_progressBar->setTaskStatus(TaskGlobal::Normal);
    } else {
        if (type == TaskManager::Removed)
            disconnect(task, nullptr, this, nullptr);

        m_lbTaskTitle->setVisible(true);
        m_progressBar->setVisible(true);
        auto firstTask = taskManager->tasks().first();
        connect(firstTask, &Task::statusUpdated, this, &MainWindow::onTaskStatusChanged);
    }
}
void MainWindow::onTaskStatusChanged(const TaskStatus &status) {
    m_lbTaskTitle->setText(status.title);
    m_progressBar->setValue(status.progress);
    m_progressBar->setTaskStatus(status.runningStatus);
    m_progressBar->setIndeterminate(status.isIndetermine);
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

        QMainWindow::closeEvent(event);
    } else if (m_isCloseRequested) {
        qDebug() << "Waiting for all tasks done...";
        if (m_waitDoneDialog) {
            m_waitDoneDialog->setMessage(tr("Please wait for all tasks done..."));
        }
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