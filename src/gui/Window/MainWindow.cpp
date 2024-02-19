//
// Created by fluty on 2024/1/31.
//

#include "MainWindow.h"

#include <QFileDialog>
#include <QSplitter>
#include <QMenuBar>

#include "Audio/AudioSystem.h"
#include "Audio/Dialogs/AudioSettingsDialog.h"
#include "Audio/Dialogs/AudioExportDialog.h"
#include "Controller/TracksViewController.h"
#include "Controller/AppController.h"
#include "Controller/ClipEditorViewController.h"
#include "Controller/ClipboardController.h"
#include "Controller/PlaybackController.h"
#include "Views/ActionButtonsView.h"
#include "Views/PlaybackView.h"
#include "Controller/History/HistoryManager.h"

#ifdef Q_OS_WIN
#  include <dwmapi.h>
#endif

MainWindow::MainWindow() {
    auto qssBase =
        "QPushButton { background: #402A2B2C; border: 1px solid #80606060; "
        "border-radius: 6px; color: #F0F0F0;padding: 4px 12px;} "
        "QPushButton:hover {background-color: #80343536; } "
        "QPushButton:pressed {background-color: #40202122 } "
        "QComboBox { background: #402A2B2C; border: 1px solid #80606060; "
        "border-radius: 6px; color: #F0F0F0;padding: 4px 12px;} "
        "QComboBox:hover {background-color: #80343536; } "
        "QComboBox QAbstractItemView { outline: 0px; border: 1px solid #80606060; color: #F0F0F0;"
        "background-color: #202122; border: 1px solid #606060; "
        "selection-background-color: #3A3B3C; "
        "border-style: none; border-radius: 4px; }"
        "QComboBox::drop-down { border: none }"
        "QComboBox::down-arrow { right: 10px;  width: 12px; height: 12px; "
        "image: url(:svg/icons/chevron_down_16_filled_white.svg)}"
        "QScrollBar::vertical{ width: 16px; background-color: #05FFFFFF; border-style: none; } "
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; } "
        "QScrollBar::handle::vertical{ border-radius: 2px; margin: 6px; background:rgba(255, 255, "
        "255, 0.25); } "
        "QScrollBar::handle::vertical::hover{ border-radius: 3px; margin: 5px; "
        "background:rgba(255, 255, 255, 0.35); } "
        "QScrollBar::handle::vertical:pressed{ background:rgba(255, 255, 255, 0.15); } "
        "QScrollBar::add-line::vertical, QScrollBar::sub-line::vertical{ border:none; } "
        "QScrollBar::horizontal{ height: 16px; background-color: #05FFFFFF; border-style: none; } "
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; } "
        "QScrollBar::handle::horizontal{ border-radius: 2px; margin: 6px; background:rgba(255, "
        "255, "
        "255, 0.25); } "
        "QScrollBar::handle::horizontal::hover{ border-radius: 3px; margin: 5px; "
        "background:rgba(255, 255, 255, 0.35); } "
        "QScrollBar::handle::horizontal:pressed{ background:rgba(255, 255, 255, 0.15); } "
        "QScrollBar::add-line::horizontal, QScrollBar::sub-line::horizontal{ border:none; } "
        "QSplitter { background-color: #2A2B2C; border: none; height: 6px } "
        "QSplitter::handle:vertical { height: 6px; background-color: #1C1D1E;} "
        "QGraphicsView { border: none; background-color: #2A2B2C;}"
        "QListWidget { background: #2A2B2C; border: none } "
        "QMenu { padding: 4px; background-color: #202122; border: 1px solid #606060; "
        "border-radius: 6px; color: #F0F0F0;} "
        "QMenu::indicator { left: 6px; width: 20px; height: 20px; } QMenu::icon { left: 6px; } "
        "QMenu::item { background: transparent; color: #F0F0F0; padding: 4px 20px; } "
        "QMenu[stats=checkable]::item, QMenu[stats=icon]::item { padding-left: 12px; } "
        "QMenu::item:selected { background-color: #3A3B3C; border: 1px solid "
        "transparent; border-style: none; border-radius: 4px; } "
        "QMenu::item:disabled { color: #d5d5d5; background-color: transparent; } "
        "QMenu::separator { height: 1.25px; background-color: #606060; margin: 6px 0; } "
        "QMenuBar {background-color: transparent; color: #F0F0F0;}"
        "QMenuBar::item {background-color: transparent; padding: 8px 12px; color: #f0f0f0; "
        "border-radius: 4px; }"
        "QMenuBar::item:selected { background-color: #10FFFFFF } "
        "QLineEdit {background: #10FFFFFF; border: 1px solid transparent; border-bottom: 1px solid "
        "#80606060;"
        "border-radius: 4px; color: #F0F0F0; selection-color: #000;"
        "selection-background-color: #9BBAFF; padding: 2px; }"
        "QLineEdit:focus { background: #202122; border: 1px solid #606060; border-bottom: 2px "
        "solid #9BBAFF; }"
        "QGraphicsView { border: none; }"
        "QDialog { background: #2A2B2C; border: 1px solid #606060; border-radius: 6px; }"
        "QCheckBox { background: transparent; color: #F0F0F0; }"
        "QCheckBox::indicator:unchecked { border: 1px solid #606060; background: #2A2B2C; }"
        "QCheckBox::indicator:checked { border: 1px solid #606060; background: #9BBAFF; }"
        "QLabel { color: #F0F0F0; }"
        "border-radius: 6px; color: #F0F0F0; padding: 4px 12px; }"
        "QTabBar::tab {background: #2A2B2C; color: #F0F0F0; border: 1px solid #606060; "
        "border-bottom: none; padding: 4px 12px; }"
        "QTabBar::tab:selected {background: #10FFFFFF; color: #F0F0F0; border: 1px solid #606060; }"
        "QTabBar::tab:unselected {background: #2A2B2C; color: #F0F0F0; border: 1px solid #606060; }"
        "QTableWidget { background: #2A2B2C; border: none; }"
        "QTextEdit { background: #10FFFFFF; border: 1px solid transparent; border-radius: 4px; "
        "color: #F0F0F0; selection-color: #000; selection-background-color: #9BBAFF; padding: 4px; "
        "}"
        "QTableView { background: #10FFFFFF; border: 1px solid transparent; border-radius: 4px; "
        "color: #F0F0F0; selection-color: #000; selection-background-color: #9BBAFF; padding: 4px; "
        "}"
        "QWidget { background: #2A2B2C; border: none; }";
    this->setStyleSheet(QString("QMainWindow { background: #232425; }") + qssBase);
#ifdef Q_OS_WIN
    // Install Windows 11 SDK 22621 if DWMWA_SYSTEMBACKDROP_TYPE is not recognized by the compiler

    bool micaOn = true;
    auto version = QSysInfo::productVersion();
    if (micaOn && version == "11") {
        // make window transparent
        this->setStyleSheet(QString("QMainWindow { background: transparent }") + qssBase);
        // Enable Mica background
        auto backDropType = DWMSBT_MAINWINDOW;
        DwmSetWindowAttribute(reinterpret_cast<HWND>(this->winId()), DWMWA_SYSTEMBACKDROP_TYPE,
                              &backDropType, sizeof(backDropType));
        // Extend title bar blur effect into client area
        constexpr int mgn = -1;
        MARGINS margins = {mgn, mgn, mgn, mgn};
        DwmExtendFrameIntoClientArea(reinterpret_cast<HWND>(this->winId()), &margins);
    }
    // Dark theme
    uint dark = 1;
    DwmSetWindowAttribute(reinterpret_cast<HWND>(this->winId()), DWMWA_USE_IMMERSIVE_DARK_MODE,
                          &dark, sizeof(dark));
#endif

    auto appController = AppController::instance();
    auto trackController = TracksViewController::instance();
    auto clipController = ClipEditorViewController::instance();
    auto playbackController = PlaybackController::instance();
    auto historyManager = HistoryManager::instance();
    auto clipboardController = ClipboardController::instance();

    auto menuBar = new QMenuBar(this);
    menuBar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto menuFile = new QMenu("&File", this);
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

    auto menuImport = new QMenu("Import", this);
    auto actionImportMidiFile = new QAction("MIDI File...", this);
    connect(actionImportMidiFile, &QAction::triggered, this, [=] {
        auto fileName =
            QFileDialog::getOpenFileName(this, "Select a MIDI File", ".", "MIDI File (*.mid)");
        if (fileName.isNull())
            return;
        appController->importMidiFile(fileName);
    });
    menuImport->addAction(actionImportMidiFile);

    auto menuExport = new QMenu("Export", this);
    auto actionExportAudio = new QAction("Audio File...", this);
    connect(actionExportAudio, &QAction::triggered, this, [=] {
        AudioExportDialog dlg;
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

    auto menuEdit = new QMenu("&Edit", this);

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

    auto menuInsert = new QMenu("&Insert", this);

    auto actionInsertNewTrack = new QAction("New track", this);
    connect(actionInsertNewTrack, &QAction::triggered, TracksViewController::instance(),
            &TracksViewController::onNewTrack);
    menuInsert->addAction(actionInsertNewTrack);

    auto menuModify = new QMenu("&Modify", this);
    auto actionFillLyrics = new QAction("Fill Lyrics", this);
    actionFillLyrics->setShortcut(QKeySequence("Ctrl+L"));
    connect(actionFillLyrics, &QAction::triggered, clipController,
            [this] { ClipEditorViewController::instance()->onFillLyric(this); });
    menuModify->addAction(actionFillLyrics);

    auto menuOptions = new QMenu("&Options", this);
    auto actionAudioSettings = new QAction("&Audio settings", this);
    connect(actionAudioSettings, &QAction::triggered, this, [=] {
        AudioSettingsDialog dlg(this);
        dlg.exec();
    });
    menuOptions->addAction(actionAudioSettings);

    auto menuHelp = new QMenu("&Help", this);
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