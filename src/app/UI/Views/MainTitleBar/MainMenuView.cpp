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
#include "Modules/Extractors/MidiExtractController.h"
#include "Modules/Extractors/PitchExtractController.h"
#include "Modules/History/HistoryManager.h"
#include "UI/Controls/Toast.h"
#include "UI/Dialogs/Audio/AudioExportDialog.h"
#include "UI/Dialogs/Extractor/ExtractPitchParamDialog.h"
#include "UI/Dialogs/Options/AppOptionsDialog.h"
#include "UI/Window/MainWindow.h"
#include "Global/AppOptionsGlobal.h"
#include "UI/Dialogs/PackageManager/PackageManagerDialog.h"
#include "Modules/RecentFiles/RecentFilesManager.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QMWidgets/cmenu.h>

MainMenuView::MainMenuView(MainWindow *mainWindow)
    : QMenuBar(mainWindow), d_ptr(new MainMenuViewPrivate(mainWindow)) {
    Q_D(MainMenuView);
    d->q_ptr = this;Ò

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    connect(appController, &AppController::activePanelChanged, this,
            [=](AppGlobal::PanelType panel) { d->onActivatedPanelChanged(panel); });

    d->initActions();
    addMenu(d->buildFileMenu());
    addMenu(d->buildEditMenu());
    addMenu(d->buildOptionsMenu());
    addMenu(d->buildHelpMenu());
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
    auto openFile = [=] {
        const auto lastDir = appController->lastProjectFolder();
        const auto fileName = QFileDialog::getOpenFileName(
            q, tr("Open"), lastDir,
            MainMenuView::tr("All Supported Files (*.dspx *.mid *.midi);;DiffScope Project File "
                             "(*.dspx);;MIDI File (*.mid *.midi)"));
        if (fileName.isNull()) {
            qDebug() << "User cancelled open";
            return;
        }
        QString errorMessage;
        appController->openFile(fileName, errorMessage);
    };
    if (!historyManager->isOnSavePoint()) {
        if (m_mainWindow->askSaveChanges())
            openFile();
    } else
        openFile();
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
    const auto lastDir = appController->lastProjectFolder();
    auto fileName = QFileDialog::getOpenFileName(q, tr("Select a MIDI File"), lastDir,
                                                 tr("MIDI File (*.mid *.midi)"));
    if (fileName.isNull())
        return;
    appController->importMidiFile(fileName);
}

void MainMenuViewPrivate::onExportMidiFile() {
    Q_Q(MainMenuView);
    const auto lastDir = appController->lastProjectFolder();
    auto fileName = QFileDialog::getSaveFileName(q, tr("Save as MIDI File"), lastDir,
                                                 tr("MIDI File (*.mid *.midi)"));
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
        enterClipEditorState();
    } else {
        exitClipEditorState();
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

// void MainMenuViewPrivate::onGetMidiFromAudioClip() {
//     const auto audioClip = dynamic_cast<AudioClip
//     *>(appModel->findClipById(dialog.selectedClipId)); Q_ASSERT(audioClip);
//     midiExtractController->runExtractMidi(audioClip);
// }

void MainMenuViewPrivate::onExtractPitchParam() {
    const auto singingClip =
        dynamic_cast<SingingClip *>(appModel->findClipById(appStatus->activeClipId));

    if (!singingClip or singingClip->clipType() != IClip::Singing) {
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
    const auto audioClip = dynamic_cast<AudioClip *>(appModel->findClipById(dialog.selectedClipId));
    Q_ASSERT(audioClip);
    pitchExtractController->runExtractPitch(audioClip, singingClip);
}

void MainMenuViewPrivate::onOctaveUp() {
    qDebug() << "MainMenuViewPrivate::onOctaveUp";
    const auto singingClip =
        dynamic_cast<SingingClip *>(appModel->findClipById(appStatus->activeClipId));
    Q_ASSERT(singingClip);

    clipController->onMoveNotes(appStatus->selectedNotes, 0, 12);
}

void MainMenuViewPrivate::onOctaveDown() {
    qDebug() << "MainMenuViewPrivate::onOctaveDown";
    const auto singingClip =
        dynamic_cast<SingingClip *>(appModel->findClipById(appStatus->activeClipId));
    Q_ASSERT(singingClip);

    clipController->onMoveNotes(appStatus->selectedNotes, 0, -12);
}

void MainMenuViewPrivate::exitApp() {
    qDebug() << "MainMenuViewPrivate::exitApp";
    m_mainWindow->close();
}

void MainMenuViewPrivate::enterClipEditorState() {
    bool hasSelectedNotes = clipController->hasSelectedNotes();

    actionSelectAll->setEnabled(clipController->canSelectAll());
    QObject::connect(clipController, &ClipController::canSelectAllChanged, actionSelectAll,
                     &QAction::setEnabled);

    actionDelete->setEnabled(hasSelectedNotes);
    QObject::connect(clipController, &ClipController::hasSelectedNotesChanged, actionDelete,
                     &QAction::setEnabled);

    actionOctaveUp->setEnabled(hasSelectedNotes);
    QObject::connect(clipController, &ClipController::hasSelectedNotesChanged, actionOctaveUp,
                     &QAction::setEnabled);

    actionOctaveDown->setEnabled(hasSelectedNotes);
    QObject::connect(clipController, &ClipController::hasSelectedNotesChanged, actionOctaveDown,
                     &QAction::setEnabled);

    actionFillLyrics->setEnabled(hasSelectedNotes);
    QObject::connect(clipController, &ClipController::hasSelectedNotesChanged, actionFillLyrics,
                     &QAction::setEnabled);
}

void MainMenuViewPrivate::exitClipEditorState() {
    QObject::disconnect(clipController, &ClipController::canSelectAllChanged, actionSelectAll,
                        &QAction::setEnabled);
    actionSelectAll->setEnabled(false);

    QObject::disconnect(clipController, &ClipController::hasSelectedNotesChanged, actionDelete,
                        &QAction::setEnabled);
    actionDelete->setEnabled(false);

    QObject::disconnect(clipController, &ClipController::hasSelectedNotesChanged, actionOctaveUp,
                        &QAction::setEnabled);
    actionOctaveUp->setEnabled(false);

    QObject::disconnect(clipController, &ClipController::hasSelectedNotesChanged, actionOctaveDown,
                        &QAction::setEnabled);
    actionOctaveDown->setEnabled(false);

    QObject::disconnect(clipController, &ClipController::hasSelectedNotesChanged, actionFillLyrics,
                        &QAction::setEnabled);
    actionFillLyrics->setEnabled(false);
}

void MainMenuViewPrivate::initActions() {
    initFileActions();
    initEditActions();
    // initOptionsActions();
    // initHelpActions();
}

void MainMenuViewPrivate::initFileActions() {
    actionNew = new QAction(tr("&New"), this);
    actionNew->setShortcut(QKeySequence("Ctrl+N"));
    connect(actionNew, &QAction::triggered, this, [this] { onNew(); });

    actionOpen = new QAction(tr("&Open..."));
    actionOpen->setShortcut(QKeySequence("Ctrl+O"));
    connect(actionOpen, &QAction::triggered, this, [this] { onOpen(); });

    actionSave = new QAction(tr("&Save"), this);
    actionSave->setShortcut(QKeySequence("Ctrl+S"));

    actionSaveAs = new QAction(tr("Save &as..."), this);
    actionSaveAs->setShortcut(QKeySequence("Ctrl+Shift+S"));

    actionImportMidi = new QAction(tr("MIDI file..."), this);
    connect(actionImportMidi, &QAction::triggered, this, [this] { onImportMidiFile(); });

    actionExportAudio = new QAction(tr("Audio file..."), this);
    connect(actionExportAudio, &QAction::triggered, this, [this] { onExportAudioFile(); });

    actionExportMidi = new QAction(tr("MIDI file..."), this);
    connect(actionExportMidi, &QAction::triggered, this, [this] { onExportMidiFile(); });

    actionOpenPackageManager = new QAction(tr("Manage packages..."), this);
    connect(actionOpenPackageManager, &QAction::triggered, this, [] {
        PackageManagerDialog dialog;
        dialog.exec();
    });

    actionExit = new QAction(tr("E&xit"), this);
    connect(actionExit, &QAction::triggered, this, [this] { exitApp(); });
}

void MainMenuViewPrivate::initEditActions() {
    Q_Q(MainMenuView);
    actionUndo = new QAction(tr("&Undo"), this);
    actionUndo->setEnabled(false);
    actionUndo->setShortcut(QKeySequence("Ctrl+Z"));
    connect(actionUndo, &QAction::triggered, historyManager, &HistoryManager::undo);

    actionRedo = new QAction(tr("&Redo"), this);
    actionRedo->setEnabled(false);
    actionRedo->setShortcut(QKeySequence("Ctrl+Y"));
    connect(actionRedo, &QAction::triggered, historyManager, &HistoryManager::redo);
    connect(historyManager, &HistoryManager::undoRedoChanged, this,
            [this](bool canUndo, const QString &undoName, bool canRedo, const QString &redoName) {
                onUndoRedoChanged(canUndo, undoName, canRedo, redoName);
            });

    actionSelectAll = new QAction(tr("Select &all"), this);
    actionSelectAll->setShortcut(QKeySequence("Ctrl+A"));
    actionSelectAll->setEnabled(false);
    connect(actionSelectAll, &QAction::triggered, this, [this] { onSelectAll(); });

    actionDelete = new QAction(tr("&Delete"), this);
    actionDelete->setShortcut(Qt::Key_Delete);
    actionDelete->setEnabled(false);
    connect(actionDelete, &QAction::triggered, this, [this] { onDelete(); });

    actionCut = new QAction(tr("Cu&t"), this);
    actionCut->setShortcut(QKeySequence("Ctrl+X"));
    connect(actionCut, &QAction::triggered, this, [this] { onCut(); });

    actionCopy = new QAction(tr("&Copy"), this);
    actionCopy->setShortcut(QKeySequence("Ctrl+C"));
    connect(actionCopy, &QAction::triggered, this, [this] { onCopy(); });

    actionPaste = new QAction(tr("&Paste"), this);
    actionPaste->setShortcut(QKeySequence("Ctrl+V"));
    connect(actionPaste, &QAction::triggered, this, [this] { onPaste(); });

    actionOctaveUp = new QAction(tr("Move an octave up"), this);
    actionOctaveUp->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Up));
    connect(actionOctaveUp, &QAction::triggered, this, [this] { onOctaveUp(); });

    actionOctaveDown = new QAction(tr("Move an octave down"), this);
    actionOctaveDown->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Down));
    connect(actionOctaveDown, &QAction::triggered, this, [this] { onOctaveDown(); });

    actionFillLyrics = new QAction(tr("Fill lyrics..."), this);
    actionFillLyrics->setShortcut(QKeySequence("Ctrl+L"));
    actionFillLyrics->setEnabled(false);
    connect(actionFillLyrics, &QAction::triggered, clipController,
            [this, q] { clipController->onFillLyric(q); });

    actionSearchLyrics = new QAction(tr("Search lyrics..."), this);
    actionSearchLyrics->setShortcut(QKeySequence("Ctrl+F"));
    connect(actionSearchLyrics, &QAction::triggered, clipController,
            [this, q] { clipController->onSearchLyric(q); });

    actionExtractPitchParam = new QAction(tr("Extract pitch parameter..."));
    connect(actionExtractPitchParam, &QAction::triggered, this, [this] { onExtractPitchParam(); });
}

CMenu *MainMenuViewPrivate::buildFileMenu() {
    Q_Q(MainMenuView);
    auto menuFile = new CMenu(tr("&File"), q);
    menuFile->addAction(actionNew);
    menuFile->addAction(actionOpen);

    menuRecentFiles = new CMenu(tr("Recent Files"), q);
    menuFile->addMenu(menuRecentFiles);
    connect(menuRecentFiles, &CMenu::aboutToShow, this, [this] { updateRecentFilesMenu(); });

    menuFile->addAction(actionSave);
    menuFile->addAction(actionSaveAs);

    menuFile->addSeparator();

    auto menuImport = new CMenu(tr("Import"), q);
    menuImport->addAction(actionImportMidi);
    menuFile->addMenu(menuImport);

    auto menuExport = new CMenu(tr("Export"), q);
    menuExport->addAction(actionExportAudio);
    menuExport->addAction(actionExportMidi);
    menuFile->addMenu(menuExport);

    menuFile->addSeparator();

    menuFile->addAction(actionOpenPackageManager);
    menuFile->addSeparator();

    menuFile->addAction(actionExit);
    return menuFile;
}

CMenu *MainMenuViewPrivate::buildEditMenu() {
    Q_Q(MainMenuView);
    auto menuEdit = new CMenu(tr("&Edit"), q);
    menuEdit->addAction(actionUndo);
    menuEdit->addAction(actionRedo);

    menuEdit->addSeparator();

    menuEdit->addAction(actionSelectAll);
    menuEdit->addAction(actionDelete);

    menuEdit->addSeparator();

    menuEdit->addAction(actionCut);
    menuEdit->addAction(actionCopy);
    menuEdit->addAction(actionPaste);

    menuEdit->addSeparator();

    menuEdit->addAction(actionOctaveUp);
    menuEdit->addAction(actionOctaveDown);

    menuEdit->addSeparator();

    menuEdit->addAction(actionFillLyrics);
    menuEdit->addAction(actionSearchLyrics);

    menuEdit->addSeparator();

    menuEdit->addAction(actionExtractPitchParam);
    return menuEdit;
}

CMenu *MainMenuViewPrivate::buildOptionsMenu() {
    Q_Q(MainMenuView);
    auto actionGeneralOptions = new QAction(tr("&General..."), this);
    connect(actionGeneralOptions, &QAction::triggered, this, [] {
        AppOptionsDialog dialog(AppOptionsGlobal::Option::General);
        dialog.exec();
    });
    auto actionAudioSettings = new QAction(tr("&Audio..."), this);
    connect(actionAudioSettings, &QAction::triggered, this, [] {
        AppOptionsDialog dialog(AppOptionsGlobal::Option::Audio);
        dialog.exec();
    });
    auto actionMidiSettings = new QAction(tr("&MIDI..."), this);
    connect(actionMidiSettings, &QAction::triggered, this, [] {
        AppOptionsDialog dialog(AppOptionsGlobal::Option::Midi);
        dialog.exec();
    });
    auto actionAppearanceOptions = new QAction(tr("A&ppearance..."), this);
    connect(actionAppearanceOptions, &QAction::triggered, this, [] {
        AppOptionsDialog dialog(AppOptionsGlobal::Option::Appearance);
        dialog.exec();
    });
    const auto actionLanguageOptions = new QAction(tr("&Language..."), this);
    connect(actionLanguageOptions, &QAction::triggered, this, [] {
        AppOptionsDialog dialog(AppOptionsGlobal::Option::Language);
        dialog.exec();
    });
    const auto actionInferenceOptions = new QAction(tr("&Inference..."), this);
    connect(actionInferenceOptions, &QAction::triggered, this, [] {
        AppOptionsDialog dialog(AppOptionsGlobal::Option::Inference);
        dialog.exec();
    });

    auto menuOptions = new CMenu(tr("&Options"), q);
    menuOptions->addAction(actionGeneralOptions);
    menuOptions->addAction(actionAudioSettings);
    menuOptions->addAction(actionMidiSettings);
    menuOptions->addAction(actionAppearanceOptions);
    menuOptions->addAction(actionLanguageOptions);
    menuOptions->addAction(actionInferenceOptions);
    return menuOptions;
}

CMenu *MainMenuViewPrivate::buildHelpMenu() {
    Q_Q(MainMenuView);
    auto actionCheckForUpdates = new QAction(tr("Check for updates"), this);
    connect(actionCheckForUpdates, &QAction::triggered, this,
            [=] { Toast::show(tr("You are already up to date")); });
    auto actionAbout = new QAction(tr("About..."), this);
    connect(actionAbout, &QAction::triggered, this, [] { Toast::show(tr("About")); });

    auto menuHelp = new CMenu(tr("&Help"), q);
    menuHelp->addAction(actionCheckForUpdates);
    menuHelp->addAction(actionAbout);
    return menuHelp;
}

// void MainMenuViewPrivate::initOptionsActions() {
// }

// void MainMenuViewPrivate::initHelpActions() {
// }

void MainMenuViewPrivate::updateRecentFilesMenu() {
    Q_Q(MainMenuView);
    if (!menuRecentFiles)
        return;

    menuRecentFiles->clear();

    const auto recentFiles = recentFilesManager->files();
    if (recentFiles.isEmpty()) {
        auto action = new QAction(tr("(Empty)"), this);
        action->setEnabled(false);
        menuRecentFiles->addAction(action);
        return;
    }

    for (const auto &filePath : recentFiles) {
        const QFileInfo fileInfo(filePath);
        if (!fileInfo.exists()) {
            continue; // 跳过不存在的文件
        }

        auto action = new QAction(fileInfo.fileName(), this);
        action->setToolTip(filePath);
        action->setData(filePath);
        connect(action, &QAction::triggered, this, [this, filePath] {
            if (!historyManager->isOnSavePoint()) {
                if (m_mainWindow->askSaveChanges()) {
                    QString errorMessage;
                    appController->openFile(filePath, errorMessage);
                }
            } else {
                QString errorMessage;
                appController->openFile(filePath, errorMessage);
            }
        });
        menuRecentFiles->addAction(action);
    }

    if (menuRecentFiles->actions().isEmpty()) {
        auto action = new QAction(tr("(Empty)"), this);
        action->setEnabled(false);
        menuRecentFiles->addAction(action);
    }
}
