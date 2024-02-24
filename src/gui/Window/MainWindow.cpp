//
// Created by fluty on 2024/1/31.
//

#include "MainWindow.h"

#include <QFile>
#include <QMenuBar>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QSplitter>

#include "Utils/WindowFrameUtils.h"
#include "Controller/AppController.h"
#include "Controller/TracksViewController.h"
#include "Controller/ClipEditorViewController.h"
#include "Controller/PlaybackController.h"
#include "Controller/History/HistoryManager.h"
#include "Controller/ClipboardController.h"
#include "Controls/Menu.h"
#include "Audio/Dialogs/AudioExportDialog.h"
#include "Audio/Dialogs/AudioSettingsDialog.h"
#include "Views/TracksEditor/TracksView.h"
#include "Views/ClipEditor/ClipEditorView.h"
#include "Views/PlaybackView.h"
#include "Views/ActionButtonsView.h"

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
    WindowFrameUtils::applyFrameEffects(this);
#endif

    auto appController = AppController::instance();
    auto trackController = TracksViewController::instance();
    auto clipController = ClipEditorViewController::instance();
    auto playbackController = PlaybackController::instance();
    auto historyManager = HistoryManager::instance();
    auto clipboardController = ClipboardController::instance();

    auto menuBar = new QMenuBar(this);
    menuBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto menuFile = new Menu("&File", this);
    auto actionNewProject = new QAction("&New Project", this);
    actionNewProject->setShortcut(QKeySequence("Ctrl+N"));
    connect(actionNewProject, &QAction::triggered, appController, &AppController::onNewProject);

    auto actionOpen = new QAction("&Open Project...");
    actionOpen->setShortcut(QKeySequence("Ctrl+O"));
    connect(actionOpen, &QAction::triggered, this, [=] {
        auto lastDir =
            appController->lastProjectPath().isEmpty() ? "." : appController->lastProjectPath();
        auto fileName = QFileDialog::getOpenFileName(this, "Select a Project File", lastDir,
                                                     "Project File (*.json)");
        if (fileName.isNull())
            return;

        appController->openProject(fileName);
    });
    auto actionOpenAProject = new QAction("Open A Project", this);
    connect(actionOpenAProject, &QAction::triggered, this, [=] {
        auto lastDir =
            appController->lastProjectPath().isEmpty() ? "." : appController->lastProjectPath();
        auto fileName = QFileDialog::getOpenFileName(this, "Select an A Project File", lastDir,
                                                     "Project File (*.json)");
        if (fileName.isNull())
            return;

        appController->importAproject(fileName);
    });

    auto actionSave = new QAction("&Save", this);
    actionSave->setShortcut(QKeySequence("Ctrl+S"));
    auto actionSaveAs = new QAction("&Save As", this);
    actionSaveAs->setShortcut(QKeySequence("Ctrl+Shift+S"));
    connect(actionSaveAs, &QAction::triggered, this, [=] {
        auto lastDir =
            appController->lastProjectPath().isEmpty() ? "." : appController->lastProjectPath();
        auto fileName = QFileDialog::getSaveFileName(this, "Save as Project File", lastDir,
                                                     "Project File (*.json)");
        if (fileName.isNull())
            return;

        appController->saveProject(fileName);
    });
    connect(actionSave, &QAction::triggered, this, [=] {
        if (appController->lastProjectPath().isEmpty()) {
            actionSaveAs->trigger();
        } else {
            appController->saveProject(appController->lastProjectPath());
        }
    });

    auto menuImport = new Menu("Import", this);
    auto actionImportMidiFile = new QAction("MIDI File...", this);
    connect(actionImportMidiFile, &QAction::triggered, this, [=] {
        auto fileName =
            QFileDialog::getOpenFileName(this, "Select a MIDI File", ".", "MIDI File (*.mid)");
        if (fileName.isNull())
            return;
        appController->importMidiFile(fileName);
    });
    menuImport->addAction(actionImportMidiFile);

    auto menuExport = new Menu("Export", this);
    auto actionExportAudio = new QAction("Audio File...", this);
    connect(actionExportAudio, &QAction::triggered, this, [=] {
        AudioExportDialog dlg(this);
        dlg.exec();
    });
    auto actionExportMidiFile = new QAction("MIDI File...", this);
    connect(actionExportMidiFile, &QAction::triggered, this, [=] {
        auto fileName =
            QFileDialog::getSaveFileName(this, "Save as MIDI File", ".", "MIDI File (*.mid)");
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

    auto menuEdit = new Menu("&Edit", this);

    auto actionUndo = new QAction("&Undo", this);
    actionUndo->setEnabled(false);
    actionUndo->setShortcut(QKeySequence("Ctrl+Z"));
    connect(actionUndo, &QAction::triggered, historyManager, &HistoryManager::undo);

    auto actionRedo = new QAction("&Redo", this);
    actionRedo->setEnabled(false);
    actionRedo->setShortcut(QKeySequence("Ctrl+Shift+Z"));
    connect(actionRedo, &QAction::triggered, historyManager, &HistoryManager::redo);
    connect(historyManager, &HistoryManager::undoRedoChanged, this,
            [=](bool canUndo, bool canRedo) {
                actionUndo->setEnabled(canUndo);
                actionRedo->setEnabled(canRedo);
            });

    auto actionSelectAll = new QAction("&Select All", this);
    actionSelectAll->setShortcut(QKeySequence("Ctrl+A"));
    connect(actionSelectAll, &QAction::triggered, clipController,
            &ClipEditorViewController::onSelectAllNotes);
    // connect(clipController, &ClipEditorViewController::canSelectAllChanged, actionSelectAll,
    //         &QAction::setEnabled);

    auto actionDelete = new QAction("&Delete", this);
    actionDelete->setShortcut(Qt::Key_Delete);
    connect(actionDelete, &QAction::triggered, clipController,
            &ClipEditorViewController::onRemoveSelectedNotes);
    // connect(clipController, &ClipEditorViewController::canRemoveChanged, actionDelete,
    //         &QAction::setEnabled);

    auto actionCut = new QAction("&Cut", this);
    actionCut->setShortcut(QKeySequence("Ctrl+X"));
    connect(actionCut, &QAction::triggered, clipboardController, &ClipboardController::cut);

    auto actionCopy = new QAction("&Copy", this);
    actionCopy->setShortcut(QKeySequence("Ctrl+C"));
    connect(actionCopy, &QAction::triggered, clipboardController, &ClipboardController::copy);

    auto actionPaste = new QAction("&Paste", this);
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

    auto menuInsert = new Menu("&Insert", this);

    auto actionInsertNewTrack = new QAction("New track", this);
    connect(actionInsertNewTrack, &QAction::triggered, TracksViewController::instance(),
            &TracksViewController::onNewTrack);
    menuInsert->addAction(actionInsertNewTrack);

    auto menuModify = new Menu("&Modify", this);
    auto actionFillLyrics = new QAction("Fill Lyrics", this);
    actionFillLyrics->setShortcut(QKeySequence("Ctrl+L"));
    connect(actionFillLyrics, &QAction::triggered, clipController,
            [this] { ClipEditorViewController::instance()->onFillLyric(this); });
    menuModify->addAction(actionFillLyrics);

    auto menuOptions = new Menu("&Options", this);
    auto actionAudioSettings = new QAction("&Audio settings", this);
    connect(actionAudioSettings, &QAction::triggered, this, [=] {
        AudioSettingsDialog dlg(this);
        dlg.exec();
    });
    menuOptions->addAction(actionAudioSettings);

    auto menuHelp = new Menu("&Help", this);
    auto actionCheckForUpdates = new QAction("Check for updates", this);
    auto actionAbout = new QAction("About", this);
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
    connect(model, &AppModel::tracksChanged, m_tracksView, &TracksView::onTrackChanged);
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
    menuBarContainer->setContentsMargins(6, 6, 6, 6);

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

    auto mainLayout = new QVBoxLayout;
    mainLayout->addLayout(actionButtonLayout);
    mainLayout->addWidget(splitter);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins({});

    auto mainWidget = new QWidget;
    mainWidget->setLayout(mainLayout);

    this->setCentralWidget(mainWidget);
    this->resize(1280, 720);
}