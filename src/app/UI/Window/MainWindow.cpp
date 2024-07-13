//
// Created by fluty on 2024/1/31.
//

#include "MainWindow.h"

#include <QMenuBar>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QSplitter>
#include <QCloseEvent>
#include <QStatusBar>
#include <QLabel>
#include <QApplication>

#include "Utils/WindowFrameUtils.h"
#include "Controller/AppController.h"
#include "Controller/TracksViewController.h"
#include "Controller/ClipEditorViewController.h"
#include "Controller/PlaybackController.h"
#include "Modules/History/HistoryManager.h"
#include "Controller/ClipboardController.h"
#include "UI/Controls/Menu.h"
#include "UI/Dialogs/Audio/AudioExportDialog.h"
#include "UI/Views/TracksEditor/TracksView.h"
#include "UI/Views/ClipEditor/ClipEditorView.h"
#include "UI/Views/PlaybackView.h"
#include "UI/Views/ActionButtonsView.h"
#include "Modules/Task/TaskManager.h"
#include "UI/Controls/ProgressIndicator.h"
#include "Model/AppOptions/AppOptions.h"
#include "UI/Dialogs/Base/TaskDialog.h"
#include "UI/Dialogs/Options/AppOptionsDialog.h"

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
    auto clipController = ClipEditorViewController::instance();
    auto playbackController = PlaybackController::instance();
    auto historyManager = HistoryManager::instance();
    auto clipboardController = ClipboardController::instance();
    auto taskManager = TaskManager::instance();

    appController->setMainWindow(this);

    connect(taskManager, &TaskManager::allDone, this, &MainWindow::onAllDone);
    connect(TaskManager::instance(), &TaskManager::taskChanged, this, &MainWindow::onTaskChanged);

    auto menuBar = new QMenuBar(this);
    menuBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto menuFile = new Menu(tr("&File"), this);
    auto actionNewProject = new QAction(tr("&New Project"), this);
    actionNewProject->setShortcut(QKeySequence("Ctrl+N"));
    connect(actionNewProject, &QAction::triggered, appController, &AppController::onNewProject);

    auto actionOpen = new QAction(tr("&Open Project..."));
    actionOpen->setShortcut(QKeySequence("Ctrl+O"));
    connect(actionOpen, &QAction::triggered, this, [=] {
        auto lastDir = appController->lastProjectFolder();
        auto fileName = QFileDialog::getOpenFileName(this, tr("Select a Project File"), lastDir,
                                                     tr("DiffScope Project File (*.dspx)"));
        if (fileName.isNull())
            return;

        appController->openProject(fileName);
    });
    auto actionOpenAProject = new QAction(tr("Open A Project"), this);
    connect(actionOpenAProject, &QAction::triggered, this, [=] {
        auto lastDir = appController->lastProjectFolder();
        auto fileName = QFileDialog::getOpenFileName(this, tr("Select an A Project File"), lastDir,
                                                     tr("Project File (*.json)"));
        if (fileName.isNull())
            return;

        appController->importAproject(fileName);
    });

    auto actionSave = new QAction(tr("&Save"), this);
    actionSave->setShortcut(QKeySequence("Ctrl+S"));
    auto actionSaveAs = new QAction(tr("Save &As..."), this);
    actionSaveAs->setShortcut(QKeySequence("Ctrl+Shift+S"));
    connect(actionSaveAs, &QAction::triggered, this, [=] {
        auto lastDir = appController->projectPath().isEmpty()
                           ? appController->lastProjectFolder() + "/" + appController->projectName()
                           : appController->projectPath();
        auto fileName = QFileDialog::getSaveFileName(this, tr("Save project"), lastDir,
                                                     tr("DiffScope Project File (*.dspx)"));
        if (fileName.isNull())
            return;

        appController->saveProject(fileName);
    });
    connect(actionSave, &QAction::triggered, this, [=] {
        if (appController->projectPath().isEmpty()) {
            actionSaveAs->trigger();
        } else {
            appController->saveProject(appController->projectPath());
        }
    });

    // auto actionCenterAt = new QAction("Center at", this);
    // connect(actionCenterAt, &QAction::triggered, this, [=] {
    //     // ClipEditorViewController::instance()->centerAt(28800, 72);
    //         auto clip = AppModel::instance()->tracks().first()->clips().at(0);
    //         if (auto singingClip = dynamic_cast<SingingClip *>(clip)) {
    //             auto note = singingClip->notes().at(m_noteIndex);
    //             ClipEditorViewController::instance()->centerAt(*note);
    //             m_noteIndex++;
    //         }
    // });

    auto menuImport = new Menu(tr("Import"), this);
    auto actionImportMidiFile = new QAction(tr("MIDI File..."), this);
    connect(actionImportMidiFile, &QAction::triggered, this, [=] {
        auto fileName = QFileDialog::getOpenFileName(this, tr("Select a MIDI File"), ".",
                                                     tr("MIDI File (*.mid)"));
        if (fileName.isNull())
            return;
        appController->importMidiFile(fileName);
    });
    menuImport->addAction(actionImportMidiFile);

    auto menuExport = new Menu(tr("Export"), this);
    auto actionExportAudio = new QAction(tr("Audio File..."), this);
    connect(actionExportAudio, &QAction::triggered, this, [=] {
        AudioExportDialog dlg(this);
        dlg.exec();
    });
    auto actionExportMidiFile = new QAction(tr("MIDI File..."), this);
    connect(actionExportMidiFile, &QAction::triggered, this, [=] {
        auto fileName = QFileDialog::getSaveFileName(this, tr("Save as MIDI File"), ".",
                                                     tr("MIDI File (*.mid)"));
        if (fileName.isNull())
            return;
        appController->exportMidiFile(fileName);
    });

    menuExport->addAction(actionExportAudio);
    menuExport->addAction(actionExportMidiFile);

    menuFile->addAction(actionNewProject);
    menuFile->addAction(actionOpen);
    menuFile->addAction(actionOpenAProject);
    menuFile->addAction(actionSave);
    menuFile->addAction(actionSaveAs);
    menuFile->addSeparator();
    menuFile->addMenu(menuImport);
    menuFile->addMenu(menuExport);
    menuFile->addSeparator();
    // menuFile->addAction(actionCenterAt);

    auto menuEdit = new Menu(tr("&Edit"), this);

    auto actionUndo = new QAction(tr("&Undo"), this);
    actionUndo->setEnabled(false);
    actionUndo->setShortcut(QKeySequence("Ctrl+Z"));
    connect(actionUndo, &QAction::triggered, historyManager, &HistoryManager::undo);

    auto actionRedo = new QAction(tr("&Redo"), this);
    actionRedo->setEnabled(false);
    actionRedo->setShortcut(QKeySequence("Ctrl+Y"));
    connect(actionRedo, &QAction::triggered, historyManager, &HistoryManager::redo);
    connect(historyManager, &HistoryManager::undoRedoChanged, this,
            [=](bool canUndo, bool canRedo) {
                actionUndo->setEnabled(canUndo);
                actionRedo->setEnabled(canRedo);
            });

    auto actionSelectAll = new QAction(tr("Select &All"), this);
    actionSelectAll->setShortcut(QKeySequence("Ctrl+A"));
    connect(actionSelectAll, &QAction::triggered, clipController,
            &ClipEditorViewController::onSelectAllNotes);
    // connect(clipController, &ClipEditorViewController::canSelectAllChanged, actionSelectAll,
    //         &QAction::setEnabled);

    auto actionDelete = new QAction(tr("&Delete"), this);
    actionDelete->setShortcut(Qt::Key_Delete);
    // TODO: fix bug
    connect(actionDelete, &QAction::triggered, clipController,
            &ClipEditorViewController::onRemoveSelectedNotes);
    // connect(clipController, &ClipEditorViewController::canRemoveChanged, actionDelete,
    //         &QAction::setEnabled);

    auto actionCut = new QAction(tr("Cu&t"), this);
    actionCut->setShortcut(QKeySequence("Ctrl+X"));
    connect(actionCut, &QAction::triggered, clipboardController, &ClipboardController::cut);

    auto actionCopy = new QAction(tr("&Copy"), this);
    actionCopy->setShortcut(QKeySequence("Ctrl+C"));
    connect(actionCopy, &QAction::triggered, clipboardController, &ClipboardController::copy);

    auto actionPaste = new QAction(tr("&Paste"), this);
    actionPaste->setShortcut(QKeySequence("Ctrl+V"));
    connect(actionPaste, &QAction::triggered, clipboardController, &ClipboardController::paste);

    menuEdit->addAction(actionUndo);
    menuEdit->addAction(actionRedo);
    menuEdit->addSeparator();
    menuEdit->addAction(actionSelectAll);
    menuEdit->addAction(actionDelete);
    menuEdit->addSeparator();
    menuEdit->addAction(actionCut);
    menuEdit->addAction(actionCopy);
    menuEdit->addAction(actionPaste);

    auto menuInsert = new Menu(tr("&Insert"), this);

    auto actionInsertNewTrack = new QAction(tr("Track"), this);
    connect(actionInsertNewTrack, &QAction::triggered, TracksViewController::instance(),
            &TracksViewController::onNewTrack);
    menuInsert->addAction(actionInsertNewTrack);

    auto menuModify = new Menu(tr("&Modify"), this);
    auto actionFillLyrics = new QAction(tr("Fill Lyrics..."), this);
    actionFillLyrics->setShortcut(QKeySequence("Ctrl+L"));
    connect(actionFillLyrics, &QAction::triggered, clipController,
            [this] { ClipEditorViewController::instance()->onFillLyric(this); });
    menuModify->addAction(actionFillLyrics);

    auto menuOptions = new Menu(tr("&Options"), this);
    auto actionGeneralOptions = new QAction(tr("&General..."), this);
    connect(actionGeneralOptions, &QAction::triggered, this, [=] {
        AppOptionsDialog dialog(AppOptionsDialog::General, this);
        dialog.exec();
    });
    auto actionAudioSettings = new QAction(tr("&Audio..."), this);
    connect(actionAudioSettings, &QAction::triggered, this, [=] {
        AppOptionsDialog dialog(AppOptionsDialog::Audio, this);
        dialog.exec();
    });
    auto actionAppearanceOptions = new QAction(tr("A&ppearance..."), this);
    connect(actionAppearanceOptions, &QAction::triggered, this, [=] {
        AppOptionsDialog dialog(AppOptionsDialog::Appearance, this);
        dialog.exec();
    });
    const auto actionLanguage = new QAction(tr("Language..."), this);
    connect(actionLanguage, &QAction::triggered, this, [=] {
        AppOptionsDialog dialog(AppOptionsDialog::Language, this);
        dialog.exec();
    });
    menuOptions->addAction(actionGeneralOptions);
    menuOptions->addAction(actionAudioSettings);
    menuOptions->addAction(actionAppearanceOptions);
    menuOptions->addAction(actionLanguage);

    auto menuHelp = new Menu(tr("&Help"), this);
    auto actionCheckForUpdates = new QAction(tr("Check for Updates"), this);
    auto actionAbout = new QAction(tr("About..."), this);
    menuHelp->addAction(actionCheckForUpdates);
    menuHelp->addAction(actionAbout);

    menuBar->addMenu(menuFile);
    menuBar->addMenu(menuEdit);
    menuBar->addMenu(menuInsert);
    menuBar->addMenu(menuModify);
    menuBar->addMenu(menuOptions);
    menuBar->addMenu(menuHelp);

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

    auto menuBarContainer = new QHBoxLayout;
    menuBarContainer->addWidget(menuBar);
    menuBarContainer->setContentsMargins(0, 6, 6, 6);

    auto actionButtonsView = new ActionButtonsView;
    connect(actionButtonsView, &ActionButtonsView::saveTriggered, actionSave, &QAction::trigger);
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