//
// Created by fluty on 2024/1/31.
//

#include <QFileDialog>
#include <QSplitter>

#include "MainWindow.h"
#include "Controller/TracksViewController.h"
#include "Controls/PianoRoll/PianoRollGraphicsView.h"
#include "Controller/AppController.h"
#include "Controller/PlaybackController.h"

#ifdef Q_OS_WIN
#  include <dwmapi.h>
#endif

MainWindow::MainWindow() {
    auto qssBase =
        "QPushButton { background: #2A2B2C; border: 1px solid #606060; "
        "border-radius: 6px; color: #F0F0F0;padding: 4px 12px;} "
        "QPushButton:hover {background-color: #343536; } "
        "QPushButton:pressed {background-color: #202122 } "
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
        "QMenu::item { background: transparent; color: #fff; padding: 4px 28px; } "
        "QMenu[stats=checkable]::item, QMenu[stats=icon]::item { padding-left: 12px; } "
        "QMenu::item:selected { color: #000; background-color: #9BBAFF; border: 1px solid "
        "transparent; "
        "border-style: none; border-radius: 4px; } "
        "QMenu::item:disabled { color: #d5d5d5; background-color: transparent; } "
        "QMenu::separator { height: 1.25px; background-color: #606060; margin: 6px 0; } ";
    this->setStyleSheet(QString("QMainWindow { background: #232425 }") + qssBase);
#ifdef Q_OS_WIN
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
    // Dark theme
    uint dark = 1;
    DwmSetWindowAttribute(reinterpret_cast<HWND>(this->winId()), DWMWA_USE_IMMERSIVE_DARK_MODE,
                          &dark, sizeof(dark));
#endif

    auto appController = AppController::instance();
    auto trackController = TracksViewController::instance();
    auto playbackController = PlaybackController::instance();

    auto btnNewTrack = new QPushButton;
    btnNewTrack->setText("New Track");
    connect(btnNewTrack, &QPushButton::clicked, trackController, &TracksViewController::onNewTrack);

    auto btnImportMidi = new QPushButton;
    btnImportMidi->setText("Import MIDI...");
    connect(btnImportMidi, &QPushButton::clicked, appController, [=]() {
        auto fileName = QFileDialog::getOpenFileName(btnImportMidi, "Select a MIDI File", ".",
                                                     "MIDI File (*.mid)");
        if (fileName.isNull())
            return;

        AppModel::instance()->importMidi(fileName);
    });

    // auto btnOpenAudioFile = new QPushButton;
    // btnOpenAudioFile->setText("Add an audio file...");
    // connect(btnOpenAudioFile, &QPushButton::clicked, trackController, [=]() {
    //     auto fileName =
    //         QFileDialog::getOpenFileName(btnOpenAudioFile, "Select an Audio File", ".",
    //                                      "All Audio File (*.wav *.flac *.mp3);;Wave File "
    //                                      "(*.wav);;Flac File (*.flac);;MP3 File (*.mp3)");
    //     if (fileName.isNull())
    //         return;
    //
    //     trackController->addAudioClipToNewTrack(fileName);
    // });

    auto btnOpenProjectFile = new QPushButton;
    btnOpenProjectFile->setText("Open project...");
    connect(btnOpenProjectFile, &QPushButton::clicked, appController, [=]() {
        auto fileName = QFileDialog::getOpenFileName(btnOpenProjectFile, "Select a Project File",
                                                     ".", "Project File (*.json)");
        if (fileName.isNull())
            return;

        appController->openProject(fileName);
    });

    auto btnPlay = new QPushButton;
    btnPlay->setText("Play");
    connect(btnPlay, &QPushButton::clicked, playbackController, [=] {
        // TODO: run project check (overlapping)
        PlaybackController::instance()->play();
    });

    auto btnPause = new QPushButton;
    btnPause->setText("Pause");
    connect(btnPause, &QPushButton::clicked, playbackController, &PlaybackController::pause);

    auto btnStop = new QPushButton;
    btnStop->setText("Stop");
    connect(btnStop, &QPushButton::clicked, playbackController, &PlaybackController::stop);

    auto m_tracksView = new TracksView;
    auto m_pianoRollView = new PianoRollGraphicsView;
    auto model = AppModel::instance();

    connect(model, &AppModel::modelChanged, m_tracksView, &TracksView::onModelChanged);
    connect(model, &AppModel::tracksChanged, m_tracksView, &TracksView::onTrackChanged);
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

    auto splitter = new QSplitter;
    splitter->setOrientation(Qt::Vertical);
    splitter->addWidget(m_tracksView);
    splitter->addWidget(m_pianoRollView);

    auto actionButtonLayout = new QHBoxLayout;
    actionButtonLayout->addWidget(btnNewTrack);
    actionButtonLayout->addWidget(btnImportMidi);
    // actionButtonLayout->addWidget(btnOpenAudioFile);
    actionButtonLayout->addWidget(btnOpenProjectFile);
    actionButtonLayout->addWidget(btnPlay);
    actionButtonLayout->addWidget(btnPause);
    actionButtonLayout->addWidget(btnStop);

    auto mainLayout = new QVBoxLayout;
    mainLayout->addLayout(actionButtonLayout);
    mainLayout->addWidget(splitter);

    auto mainWidget = new QWidget;
    mainWidget->setLayout(mainLayout);

    this->setCentralWidget(mainWidget);
    this->resize(1280, 720);
}