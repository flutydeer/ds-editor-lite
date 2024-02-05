//
// Created by fluty on 2024/1/31.
//

#include "MainWindow.h"

#include <QFileDialog>
#include <QSplitter>
#include <QMenu>
#include <QMenuBar>

#include "Audio/AudioSystem.h"
#include "Controller/TracksViewController.h"
#include "Controls/PianoRoll/PianoRollGraphicsView.h"
#include "Controller/AppController.h"
#include "Controller/PlaybackController.h"
#include "Views/PlaybackView.h"

#ifdef Q_OS_WIN
#  include <dwmapi.h>
#endif

MainWindow::MainWindow() {
    auto qssBase =
        "QPushButton { background: #402A2B2C; border: 1px solid #80606060; "
        "border-radius: 6px; color: #F0F0F0;padding: 4px 12px;} "
        "QPushButton:hover {background-color: #80343536; } "
        "QPushButton:pressed {background-color: #40202122 } "
        "QScrollBar::vertical{ width:10px; background-color: transparent; border-style: none; "
        "border-radius: 4px; } "
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; } "
        "QScrollBar::handle::vertical{ border-radius:4px; width: 10px; background:rgba(255, 255, "
        "255, 0.2); } "
        "QScrollBar::handle::vertical::hover{ background:rgba(255, 255, 255, 0.3); } "
        "QScrollBar::handle::vertical:pressed{ background:rgba(255, 255, 255, 0.1); } "
        "QScrollBar::add-line::vertical, QScrollBar::sub-line::vertical{ border:none; } "
        "QScrollBar::horizontal{ height:10px; background-color: transparent; border-style: none; "
        "border-radius: 4px; } "
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; } "
        "QScrollBar::handle::horizontal{ border-radius:4px; width: 10px; background:rgba(255, 255, "
        "255, 0.2); } "
        "QScrollBar::handle::horizontal::hover{ background:rgba(255, 255, 255, 0.3); } "
        "QScrollBar::handle::horizontal:pressed{ background:rgba(255, 255, 255, 0.1); } "
        "QScrollBar::add-line::horizontal, QScrollBar::sub-line::horizontal{ border:none; } "
        "QSplitter { background-color: transparent; border: none; } "
        "QSplitter::handle { background-color: transparent; margin: 0px 4px; } "
        "QSplitterHandle::item:hover {} QSplitter::handle:hover { background-color: rgb(155, 186, "
        "255); } "
        "QSplitter::handle:pressed { background-color: rgb(112, 156, 255); } "
        "QSplitter::handle:horizontal { width: "
        "4px; } "
        "QSplitter::handle:vertical { height: 6px; } "
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
        "QMenuBar::item {background-color: transparent; padding: 8px; color: #f0f0f0; border-radius: 4px; }"
        "QMenuBar::item:selected { background-color: #10FFFFFF } ";
    this->setStyleSheet(QString("QMainWindow { background: #232425 }") + qssBase);
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
    auto playbackController = PlaybackController::instance();

    auto menuBar = new QMenuBar(this);

    auto menuFile = new QMenu("&File", this);
    auto actionNewProject = new QAction("&New project", this);
    connect(actionNewProject, &QAction::triggered, appController, &AppController::onNewProject);
    auto actionOpen = new QAction("&Open project...");
    connect(actionOpen, &QAction::triggered, this, [=] {
        auto fileName = QFileDialog::getOpenFileName(this, "Select a Project File",
                                                     ".", "Project File (*.json)");
        if (fileName.isNull())
            return;

        appController->openProject(fileName);
    });
    auto actionSave = new QAction("&Save", this);
    auto actionSaveAs = new QAction("&Save as", this);

    auto menuImport = new QMenu("Import", this);
    auto actionImportMidiFile = new QAction("MIDI file...", this);
    connect(actionImportMidiFile, &QAction::triggered, this, [=] {
        auto fileName = QFileDialog::getOpenFileName(this, "Select a MIDI File", ".",
                                                     "MIDI File (*.mid)");
        if (fileName.isNull())
            return;
        appController->importMidiFile(fileName);
    });
    menuImport->addAction(actionImportMidiFile);

    auto menuExport = new QMenu("Export", this);
    auto actionExportAudio = new QAction("Audio file...", this);
    auto actionExportMidiFile = new QAction("MIDI file...", this);

    auto actionAudioSettings = new QAction("Audio settings...", this);

    menuExport->addAction(actionExportAudio);
    menuExport->addAction(actionExportMidiFile);

    menuFile->addAction(actionNewProject);
    menuFile->addAction(actionOpen);
    menuFile->addAction(actionSave);
    menuFile->addAction(actionSaveAs);
    menuFile->addSeparator();
    menuFile->addMenu(menuImport);
    menuFile->addMenu(menuExport);

    auto menuEdit = new QMenu("&Edit", this);

    auto actionUndo = new QAction("&Undo", this);
    auto actionRedo = new QAction("&Redo", this);
    auto actionCut = new QAction("&Cut", this);
    auto actionCopy = new QAction("&Copy", this);
    auto actionPaste = new QAction("&Paste", this);
    auto actionDelete = new QAction("&Delete", this);

    menuEdit->addAction(actionUndo);
    menuEdit->addAction(actionRedo);
    menuEdit->addSeparator();
    menuEdit->addAction(actionCut);
    menuEdit->addAction(actionCopy);
    menuEdit->addAction(actionPaste);
    menuEdit->addAction(actionDelete);

    menuBar->addMenu(menuFile);
    menuBar->addMenu(menuEdit);

    auto m_tracksView = new TracksView;
    auto m_pianoRollView = new PianoRollGraphicsView;
    auto model = AppModel::instance();

    connect(model, &AppModel::modelChanged, m_tracksView, &TracksView::onModelChanged);
    connect(model, &AppModel::tracksChanged, m_tracksView, &TracksView::onTrackChanged);
    connect(model, &AppModel::tempoChanged, m_tracksView, &TracksView::onTempoChanged);
    connect(model, &AppModel::modelChanged, m_pianoRollView, &PianoRollGraphicsView::updateView);
    connect(model, &AppModel::selectedClipChanged, m_pianoRollView,
            &PianoRollGraphicsView::onSelectedClipChanged);

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
    connect(m_tracksView, &TracksView::clipPropertyChanged, trackController,
            &TracksViewController::onClipPropertyChanged);
    connect(m_tracksView, &TracksView::removeClipTriggered, trackController,
            &TracksViewController::onRemoveClip);

    auto splitter = new QSplitter;
    splitter->setOrientation(Qt::Vertical);
    splitter->addWidget(m_tracksView);
    splitter->addWidget(m_pianoRollView);

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
    menuBarContainer->setContentsMargins(6,6,6,6);

    auto actionButtonLayout = new QHBoxLayout;
    actionButtonLayout->addLayout(menuBarContainer);
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