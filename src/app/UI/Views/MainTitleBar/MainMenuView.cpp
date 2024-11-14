//
// Created by fluty on 2024/7/13.
//

#include "MainMenuView.h"

#include "MainMenuView_p.h"
#include "Controller/AppController.h"
#include "Controller/ClipController.h"
#include "Controller/TrackController.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/AudioClip.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Extractors/PitchExtractController.h"
#include "Modules/History/HistoryManager.h"
#include "UI/Controls/Toast.h"
#include "UI/Dialogs/Audio/AudioExportDialog.h"
#include "UI/Dialogs/Extractor/ExtractPitchParamDialog.h"
#include "UI/Dialogs/Options/AppOptionsDialog.h"
#include "UI/Window/MainWindow.h"

#include <QFileDialog>
#include <QMWidgets/cmenu.h>

MainMenuView::MainMenuView(MainWindow *mainWindow)
    : QMenuBar(mainWindow), d_ptr(new MainMenuViewPrivate(mainWindow)) {
    Q_D(MainMenuView);
    d->q_ptr = this;

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    connect(appController, &AppController::activePanelChanged, this,
            [=](AppGlobal::PanelType panel) { d->onActivatedPanelChanged(panel); });

    auto menuFile = new CMenu(tr("&File"), this);
    auto actionNew = new QAction(tr("&New"), this);
    actionNew->setShortcut(QKeySequence("Ctrl+N"));
    connect(actionNew, &QAction::triggered, this, [=] { d->onNew(); });

    auto actionOpen = new QAction(tr("&Open..."));
    actionOpen->setShortcut(QKeySequence("Ctrl+O"));
    connect(actionOpen, &QAction::triggered, this, [=] { d->onOpen(); });

    // auto actionOpenAProject = new QAction(tr("Open A Project"), this);
    // connect(actionOpenAProject, &QAction::triggered, this, [=] { d->onOpenAProject(); });

    d->actionSave = new QAction(tr("&Save"), this);
    d->actionSave->setShortcut(QKeySequence("Ctrl+S"));
    d->actionSaveAs = new QAction(tr("Save &As..."), this);
    d->actionSaveAs->setShortcut(QKeySequence("Ctrl+Shift+S"));

    auto menuImport = new CMenu(tr("Import"), this);
    auto actionImportMidiFile = new QAction(tr("MIDI File..."), this);
    connect(actionImportMidiFile, &QAction::triggered, this, [=] { d->onImportMidiFile(); });
    menuImport->addAction(actionImportMidiFile);

    auto menuExport = new CMenu(tr("Export"), this);
    auto actionExportAudio = new QAction(tr("Audio File..."), this);
    connect(actionExportAudio, &QAction::triggered, this, [=] { d->onExportAudioFile(); });
    auto actionExportMidiFile = new QAction(tr("MIDI File..."), this);
    connect(actionExportMidiFile, &QAction::triggered, this, [=] { d->onExportMidiFile(); });

    menuExport->addAction(actionExportAudio);
    menuExport->addAction(actionExportMidiFile);

    auto actionExit = new QAction(tr("Exit"), this);
    connect(actionExit, &QAction::triggered, this, [=] { d->exitApp(); });

    menuFile->addAction(actionNew);
    menuFile->addAction(actionOpen);
    // menuFile->addAction(actionOpenAProject);
    menuFile->addAction(d->actionSave);
    menuFile->addAction(d->actionSaveAs);
    menuFile->addSeparator();
    menuFile->addMenu(menuImport);
    menuFile->addMenu(menuExport);
    menuFile->addSeparator();
    menuFile->addAction(actionExit);

    auto menuEdit = new CMenu(tr("&Edit"), this);

    d->actionUndo = new QAction(tr("&Undo"), this);
    d->actionUndo->setEnabled(false);
    d->actionUndo->setShortcut(QKeySequence("Ctrl+Z"));
    connect(d->actionUndo, &QAction::triggered, historyManager, &HistoryManager::undo);

    d->actionRedo = new QAction(tr("&Redo"), this);
    d->actionRedo->setEnabled(false);
    d->actionRedo->setShortcut(QKeySequence("Ctrl+Y"));
    connect(d->actionRedo, &QAction::triggered, historyManager, &HistoryManager::redo);
    connect(historyManager, &HistoryManager::undoRedoChanged, this,
            [=](bool canUndo, const QString &undoName, bool canRedo, const QString &redoName) {
                d->onUndoRedoChanged(canUndo, undoName, canRedo, redoName);
            });

    d->actionSelectAll = new QAction(tr("Select &All"), this);
    d->actionSelectAll->setShortcut(QKeySequence("Ctrl+A"));
    d->actionSelectAll->setEnabled(false);
    connect(d->actionSelectAll, &QAction::triggered, this, [=] { d->onSelectAll(); });

    d->actionDelete = new QAction(tr("&Delete"), this);
    d->actionDelete->setShortcut(Qt::Key_Delete);
    d->actionDelete->setEnabled(false);
    connect(d->actionDelete, &QAction::triggered, this, [=] { d->onDelete(); });

    d->actionCut = new QAction(tr("Cu&t"), this);
    d->actionCut->setShortcut(QKeySequence("Ctrl+X"));
    connect(d->actionCut, &QAction::triggered, this, [=] { d->onCut(); });

    d->actionCopy = new QAction(tr("&Copy"), this);
    d->actionCopy->setShortcut(QKeySequence("Ctrl+C"));
    connect(d->actionCopy, &QAction::triggered, this, [=] { d->onCopy(); });

    d->actionPaste = new QAction(tr("&Paste"), this);
    d->actionPaste->setShortcut(QKeySequence("Ctrl+V"));
    connect(d->actionPaste, &QAction::triggered, this, [=] { d->onPaste(); });

    menuEdit->addAction(d->actionUndo);
    menuEdit->addAction(d->actionRedo);
    menuEdit->addSeparator();
    menuEdit->addAction(d->actionSelectAll);
    menuEdit->addAction(d->actionDelete);
    menuEdit->addSeparator();
    menuEdit->addAction(d->actionCut);
    menuEdit->addAction(d->actionCopy);
    menuEdit->addAction(d->actionPaste);

    // auto menuInsert = new CMenu(tr("&Insert"), this);
    //
    // auto actionInsertNewTrack = new QAction(tr("Track"), this);
    // connect(actionInsertNewTrack, &QAction::triggered, trackController,
    //         &TrackController::onNewTrack);
    // menuInsert->addAction(actionInsertNewTrack);

    // auto menuModify = new CMenu(tr("&Modify"), this);
    d->actionFillLyrics = new QAction(tr("Fill Lyrics..."), this);
    d->actionFillLyrics->setShortcut(QKeySequence("Ctrl+L"));
    d->actionFillLyrics->setEnabled(false);
    connect(d->actionFillLyrics, &QAction::triggered, clipController,
            [this] { clipController->onFillLyric(this); });

    d->actionSearchLyrics = new QAction(tr("Search Lyrics..."), this);
    d->actionSearchLyrics->setShortcut(QKeySequence("Ctrl+F"));
    connect(d->actionSearchLyrics, &QAction::triggered, clipController,
            [this] { clipController->onSearchLyric(this); });
    // menuModify->addAction(d->m_actionFillLyrics);

    d->actionGetPitchParamFromAudioClip = new QAction(tr("Get pitch parameter from audio clip..."));
    connect(d->actionGetPitchParamFromAudioClip, &QAction::triggered, this,
            [=] { d->onGetPitchParamFromAudioClip(); });
    menuEdit->addSeparator();
    menuEdit->addAction(d->actionFillLyrics);
    menuEdit->addAction(d->actionSearchLyrics);
    menuEdit->addSeparator();
    menuEdit->addAction(d->actionGetPitchParamFromAudioClip);

    auto menuOptions = new CMenu(tr("&Options"), this);
    auto actionGeneralOptions = new QAction(tr("&General..."), this);
    connect(actionGeneralOptions, &QAction::triggered, this, [=] {
        AppOptionsDialog dialog(AppOptionsDialog::General);
        dialog.exec();
    });
    auto actionAudioSettings = new QAction(tr("&Audio..."), this);
    connect(actionAudioSettings, &QAction::triggered, this, [=] {
        AppOptionsDialog dialog(AppOptionsDialog::Audio);
        dialog.exec();
    });
    auto actionMidiSettings = new QAction(tr("&MIDI..."), this);
    connect(actionMidiSettings, &QAction::triggered, this, [=] {
        AppOptionsDialog dialog(AppOptionsDialog::Midi);
        dialog.exec();
    });
    auto actionAppearanceOptions = new QAction(tr("A&ppearance..."), this);
    connect(actionAppearanceOptions, &QAction::triggered, this, [=] {
        AppOptionsDialog dialog(AppOptionsDialog::Appearance);
        dialog.exec();
    });
    const auto actionLanguageOptions = new QAction(tr("&G2p..."), this);
    connect(actionLanguageOptions, &QAction::triggered, this, [=] {
        AppOptionsDialog dialog(AppOptionsDialog::G2p);
        dialog.exec();
    });
    const auto actionInferenceOptions = new QAction(tr("&Inference..."), this);
    connect(actionInferenceOptions, &QAction::triggered, this, [=] {
        AppOptionsDialog dialog(AppOptionsDialog::Inference);
        dialog.exec();
    });
    menuOptions->addAction(actionGeneralOptions);
    menuOptions->addAction(actionAudioSettings);
    menuOptions->addAction(actionMidiSettings);
    menuOptions->addAction(actionAppearanceOptions);
    menuOptions->addAction(actionLanguageOptions);
    menuOptions->addAction(actionInferenceOptions);

    auto menuHelp = new CMenu(tr("&Help"), this);
    auto actionCheckForUpdates = new QAction(tr("Check for Updates"), this);
    connect(actionCheckForUpdates, &QAction::triggered, this,
            [=] { Toast::show(tr("You are already up to date")); });
    auto actionAbout = new QAction(tr("About..."), this);
    connect(actionAbout, &QAction::triggered, this, [=] { Toast::show(tr("About")); });
    menuHelp->addAction(actionCheckForUpdates);
    menuHelp->addAction(actionAbout);

    addMenu(menuFile);
    addMenu(menuEdit);
    // addMenu(menuInsert);
    // addMenu(menuModify);
    addMenu(menuOptions);
    addMenu(menuHelp);
}

MainMenuView::~MainMenuView() = default;

QAction *MainMenuView::actionSave() {
    Q_D(MainMenuView);
    return d->actionSave;
}

QAction *MainMenuView::actionSaveAs() {
    Q_D(MainMenuView);
    return d->actionSaveAs;
}

void MainMenuViewPrivate::onNew() const {
    if (!historyManager->isOnSavePoint()) {
        if (m_mainWindow->askSaveChanges())
            appController->newProject();
    } else
        appController->newProject();
}

void MainMenuViewPrivate::onOpen() {
    Q_Q(MainMenuView);
    auto openProject = [=] {
        auto lastDir = appController->lastProjectFolder();
        auto fileName =
            QFileDialog::getOpenFileName(q, tr("Select a Project File"), lastDir,
                                         MainMenuView::tr("DiffScope Project File (*.dspx)"));
        if (fileName.isNull())
            return;
        appController->openProject(fileName);
    };
    if (!historyManager->isOnSavePoint()) {
        if (m_mainWindow->askSaveChanges())
            openProject();
    } else
        openProject();
}

// void MainMenuViewPrivate::onOpenAProject() {
//     Q_Q(MainMenuView);
//     auto openAProject = [=] {
//         auto lastDir = appController->lastProjectFolder();
//         auto fileName = QFileDialog::getOpenFileName(q, tr("Select an A Project File"), lastDir,
//                                                      tr("Project File (*.json)"));
//         if (fileName.isNull())
//             return;
//         appController->importAceProject(fileName);
//     };
//     if (!historyManager->isOnSavePoint()) {
//         if (m_mainWindow->askSaveChanges())
//             openAProject();
//     } else
//         openAProject();
// }

void MainMenuViewPrivate::onImportMidiFile() {
    Q_Q(MainMenuView);
    auto fileName =
        QFileDialog::getOpenFileName(q, tr("Select a MIDI File"), ".", tr("MIDI File (*.mid)"));
    if (fileName.isNull())
        return;
    appController->importMidiFile(fileName);
}

void MainMenuViewPrivate::onExportMidiFile() {
    Q_Q(MainMenuView);
    auto fileName =
        QFileDialog::getSaveFileName(q, tr("Save as MIDI File"), ".", tr("MIDI File (*.mid)"));
    if (fileName.isNull())
        return;
    appController->exportMidiFile(fileName);
}

void MainMenuViewPrivate::onExportAudioFile() {
    Q_Q(MainMenuView);
    AudioExportDialog dlg(q);
    dlg.exec();
}

void MainMenuViewPrivate::onUndoRedoChanged(bool canUndo, const QString &undoName, bool canRedo,
                                            const QString &redoName) {
    Q_Q(MainMenuView);
    actionUndo->setEnabled(canUndo);
    actionUndo->setText(tr("&Undo") + " " + undoName);
    actionRedo->setEnabled(canRedo);
    actionRedo->setText(tr("&Redo") + " " + redoName);
}

void MainMenuViewPrivate::onActivatedPanelChanged(AppGlobal::PanelType panel) {
    Q_Q(MainMenuView);
    m_panelType = panel;
    if (panel == AppGlobal::ClipEditor) {
        actionSelectAll->setEnabled(clipController->canSelectAll());
        QObject::connect(clipController, &ClipController::canSelectAllChanged, actionSelectAll,
                         &QAction::setEnabled);

        actionDelete->setEnabled(clipController->hasSelectedNotes());
        QObject::connect(clipController, &ClipController::hasSelectedNotesChanged, actionDelete,
                         &QAction::setEnabled);

        actionFillLyrics->setEnabled(clipController->hasSelectedNotes());
        QObject::connect(clipController, &ClipController::hasSelectedNotesChanged, actionFillLyrics,
                         &QAction::setEnabled);
    } else {
        QObject::disconnect(clipController, &ClipController::canSelectAllChanged, actionSelectAll,
                            &QAction::setEnabled);
        actionSelectAll->setEnabled(false);

        QObject::disconnect(clipController, &ClipController::hasSelectedNotesChanged, actionDelete,
                            &QAction::setEnabled);
        actionDelete->setEnabled(false);

        QObject::disconnect(clipController, &ClipController::hasSelectedNotesChanged,
                            actionFillLyrics, &QAction::setEnabled);
        actionFillLyrics->setEnabled(false);
    }
}

void MainMenuViewPrivate::onSelectAll() {
    Q_Q(MainMenuView);
    qDebug() << "MainMenuView::onSelectAll";
    if (m_panelType == AppGlobal::ClipEditor)
        clipController->onSelectAllNotes();
}

void MainMenuViewPrivate::onDelete() {
    Q_Q(MainMenuView);
    qDebug() << "MainMenuView::onDelete";
    if (m_panelType == AppGlobal::ClipEditor)
        clipController->onDeleteSelectedNotes();
}

void MainMenuViewPrivate::onCut() {
    qDebug() << "MainMenuView::onCut";
}

void MainMenuViewPrivate::onCopy() {
    qDebug() << "MainMenuView::onCopy";
}

void MainMenuViewPrivate::onPaste() {
    qDebug() << "MainMenuView::onPaste";
}

void MainMenuViewPrivate::onGetPitchParamFromAudioClip() {
    auto singingClip = dynamic_cast<SingingClip *>(appModel->findClipById(appStatus->activeClipId));
    Q_ASSERT(singingClip);
    if (singingClip->clipType() != IClip::Singing) {
        // TODO: 在选中非歌声剪辑时禁用此操作
        Toast::show("请先选中一个歌声剪辑");
        return;
    }

    QList<AudioClip *> clips;
    for (const auto track : appModel->tracks())
        for (auto clip : track->clips())
            if (clip->clipType() == IClip::Audio) {
                auto audioClip = dynamic_cast<AudioClip *>(clip);
                Q_ASSERT(audioClip);
                clips.append(audioClip);
            }
    if (clips.isEmpty()) {
        Toast::show("请先添加一个音频文件");
        return;
    }

    ExtractPitchParamDialog dialog(clips);
    dialog.exec();
    if (dialog.selectedClipId == -1) {
        qDebug() << "User canceled get pitch param from audio clip";
        return;
    }
    auto audioClip = dynamic_cast<AudioClip *>(appModel->findClipById(dialog.selectedClipId));
    Q_ASSERT(audioClip);
    pitchExtractController->runExtractPitch(audioClip, singingClip);
}

void MainMenuViewPrivate::exitApp() {
    qDebug() << "MainMenuViewPrivate::exitApp";
    m_mainWindow->close();
}