//
// Created by fluty on 2024/1/31.
//

#include "MainWindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QSplitter>
#include <QStatusBar>

#include "Controller/AppController.h"
#include "Controller/PlaybackController.h"
#include "Controller/TracksViewController.h"
#include "Modules/History/HistoryManager.h"
#include "Modules/Task/TaskManager.h"
#include "UI/Controls/ProgressIndicator.h"
#include "UI/Dialogs/Base/TaskDialog.h"
#include "UI/Views/ActionButtonsView.h"
#include "UI/Views/PlaybackView.h"
#include "UI/Views/ClipEditor/ClipEditorView.h"
#include "UI/Views/MainMenu/MainMenuView.h"
#include "UI/Views/TracksEditor/TracksView.h"
#include "Utils/WindowFrameUtils.h"

MainWindow::MainWindow() {
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

    auto appController = AppController::instance();
    auto trackController = TracksViewController::instance();
    auto playbackController = PlaybackController::instance();
    auto historyManager = HistoryManager::instance();
    auto taskManager = TaskManager::instance();

    appController->setMainWindow(this);

    connect(taskManager, &TaskManager::allDone, this, &MainWindow::onAllDone);
    connect(TaskManager::instance(), &TaskManager::taskChanged, this, &MainWindow::onTaskChanged);

    m_tracksView = new TracksView;
    m_clipEditView = new ClipEditorView;
    auto model = AppModel::instance();

    connect(model, &AppModel::modelChanged, m_tracksView, &TracksView::onModelChanged);
    connect(model, &AppModel::trackChanged, m_tracksView, &TracksView::onTrackChanged);
    connect(model, &AppModel::tempoChanged, m_tracksView, &TracksView::onTempoChanged);
    connect(model, &AppModel::modelChanged, m_clipEditView, &ClipEditorView::onModelChanged);
    connect(model, &AppModel::selectedClipChanged, m_clipEditView,
            &ClipEditorView::onSelectedClipChanged);

    connect(m_tracksView, &TracksView::selectedClipChanged, trackController,
            &TracksViewController::onSelectedClipChanged);
    connect(m_tracksView, &TracksView::trackPropertyChanged, trackController,
            &TracksViewController::onTrackPropertyChanged);
    connect(m_tracksView, &TracksView::insertNewTrackTriggered, trackController,
            &TracksViewController::onInsertNewTrack);
    connect(m_tracksView, &TracksView::removeTrackTriggerd, trackController,
            &TracksViewController::onRemoveTrack);
    connect(m_tracksView, &TracksView::addAudioClipTriggered, trackController,
            &TracksViewController::onAddAudioClip);
    connect(m_tracksView, &TracksView::newSingingClipTriggered, trackController,
            &TracksViewController::onNewSingingClip);
    connect(m_tracksView, &TracksView::clipPropertyChanged, trackController,
            &TracksViewController::onClipPropertyChanged);
    connect(m_tracksView, &TracksView::removeClipTriggered, trackController,
            &TracksViewController::onRemoveClip);

    auto splitter = new QSplitter;
    splitter->setOrientation(Qt::Vertical);
    splitter->addWidget(m_tracksView);
    splitter->addWidget(m_clipEditView);

    auto playbackView = new PlaybackView;
    connect(playbackView, &PlaybackView::setTempoTriggered, appController,
            &AppController::onSetTempo);
    connect(playbackView, &PlaybackView::setTimeSignatureTriggered, appController,
            &AppController::onSetTimeSignature);
    connect(playbackView, &PlaybackView::playTriggered, playbackController,
            &PlaybackController::play);
    connect(playbackView, &PlaybackView::pauseTriggered, playbackController,
            &PlaybackController::pause);
    connect(playbackView, &PlaybackView::stopTriggered, playbackController,
            &PlaybackController::stop);
    connect(playbackView, &PlaybackView::setPositionTriggered, playbackController, [=](int tick) {
        playbackController->setLastPosition(tick);
        playbackController->setPosition(tick);
    });
    connect(playbackView, &PlaybackView::setQuantizeTriggered, appController,
            &AppController::onSetQuantize);
    connect(playbackController, &PlaybackController::playbackStatusChanged, playbackView,
            &PlaybackView::onPlaybackStatusChanged);
    connect(playbackController, &PlaybackController::positionChanged, playbackView,
            &PlaybackView::onPositionChanged);
    connect(model, &AppModel::modelChanged, playbackView, &PlaybackView::updateView);
    connect(model, &AppModel::tempoChanged, playbackView, &PlaybackView::onTempoChanged);
    connect(model, &AppModel::timeSignatureChanged, playbackView,
            &PlaybackView::onTimeSignatureChanged);
    playbackView->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto mainMenu = new MainMenuView(this);
    auto menuBarContainer = new QHBoxLayout;
    menuBarContainer->addWidget(mainMenu);
    menuBarContainer->setContentsMargins(0, 6, 6, 6);

    auto actionButtonsView = new ActionButtonsView;
    connect(actionButtonsView, &ActionButtonsView::saveTriggered, mainMenu->actionSave(), &QAction::trigger);
    connect(actionButtonsView, &ActionButtonsView::undoTriggered, historyManager,
            &HistoryManager::undo);
    connect(actionButtonsView, &ActionButtonsView::redoTriggered, historyManager,
            &HistoryManager::redo);
    connect(historyManager, &HistoryManager::undoRedoChanged, actionButtonsView,
            &ActionButtonsView::onUndoRedoChanged);

    AppController::instance()->onNewProject();

    auto actionButtonLayout = new QHBoxLayout;
    actionButtonLayout->addLayout(menuBarContainer);
    actionButtonLayout->addWidget(actionButtonsView);
    actionButtonLayout->addSpacerItem(new QSpacerItem(20, 20, QSizePolicy::Expanding));
    actionButtonLayout->addWidget(playbackView);
    actionButtonLayout->setContentsMargins({});

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
    mainLayout->addLayout(actionButtonLayout);
    mainLayout->addWidget(splitter);
    mainLayout->addWidget(statusBar);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins({6, 0, 6, 0});

    auto mainWidget = new QWidget;
    mainWidget->setLayout(mainLayout);

    this->setCentralWidget(mainWidget);
    this->resize(1280, 720);

    WindowFrameUtils::applyFrameEffects(this);
}
void MainWindow::setProjectName(const QString &name) {
    auto appName = qApp->applicationDisplayName();
    if (name.isNull() || name.isEmpty())
        setWindowTitle(appName);
    else
        setWindowTitle(name + " - " + appName);
}
void MainWindow::onAllDone() {
    if (m_isCloseRequested) {
        m_isAllDone = true;
        close();
    }
}
void MainWindow::onTaskChanged(TaskManager::TaskChangeType type, Task *task, qsizetype index) {
    auto taskCount = TaskManager::instance()->tasks().count();
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
        auto firstTask = TaskManager::instance()->tasks().first();
        connect(firstTask, &Task::statusUpdated, this, &MainWindow::onTaskStatusChanged);
    }
}
void MainWindow::onTaskStatusChanged(const TaskStatus &status) {
    m_lbTaskTitle->setText(status.title);
    m_progressBar->setValue(status.progress);
    m_progressBar->setTaskStatus(status.runningStatus);
    m_progressBar->setIndeterminate(status.isIndetermine);
}
void MainWindow::closeEvent(QCloseEvent *event) {
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

        auto taskManager = TaskManager::instance();
        taskManager->terminateAllTasks();
        auto thread = new QThread;
        taskManager->moveToThread(thread);
        connect(thread, &QThread::started, taskManager, &TaskManager::wait);
        thread->start();
        event->ignore();
    }
    // taskManager->wait();
    // QMainWindow::closeEvent(event);
}