//
// Created by fluty on 2024/7/13.
//

#include "MainMenuView.h"

#include "Controller/AppController.h"
#include "Controller/ClipEditorViewController.h"
#include "Controller/ClipboardController.h"
#include "Controller/TracksViewController.h"
#include "Modules/History/HistoryManager.h"
#include "UI/Controls/Menu.h"
#include "UI/Controls/Toast.h"
#include "UI/Dialogs/Audio/AudioExportDialog.h"
#include "UI/Dialogs/Options/AppOptionsDialog.h"

#include <QFileDialog>

MainMenuView::MainMenuView(QWidget *parent) : QMenuBar(parent) {
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto appController = AppController::instance();
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

    m_actionSave = new QAction(tr("&Save"), this);
    m_actionSave->setShortcut(QKeySequence("Ctrl+S"));
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
    connect(m_actionSave, &QAction::triggered, this, [=] {
        if (appController->projectPath().isEmpty()) {
            actionSaveAs->trigger();
        } else {
            appController->saveProject(appController->projectPath());
        }
    });

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
    menuFile->addAction(m_actionSave);
    menuFile->addAction(actionSaveAs);
    menuFile->addSeparator();
    menuFile->addMenu(menuImport);
    menuFile->addMenu(menuExport);
    menuFile->addSeparator();

    auto menuEdit = new Menu(tr("&Edit"), this);

    auto historyManager = HistoryManager::instance();
    auto actionUndo = new QAction(tr("&Undo"), this);
    actionUndo->setEnabled(false);
    actionUndo->setShortcut(QKeySequence("Ctrl+Z"));
    connect(actionUndo, &QAction::triggered, historyManager, &HistoryManager::undo);

    auto actionRedo = new QAction(tr("&Redo"), this);
    actionRedo->setEnabled(false);
    actionRedo->setShortcut(QKeySequence("Ctrl+Y"));
    connect(actionRedo, &QAction::triggered, historyManager, &HistoryManager::redo);
    connect(historyManager, &HistoryManager::undoRedoChanged, this,
            [=](bool canUndo, const QString &undoActionName, bool canRedo,
                const QString &redoActionName) {
                actionUndo->setEnabled(canUndo);
                actionUndo->setText(tr("&Undo") + " " + undoActionName);
                actionRedo->setEnabled(canRedo);
                actionRedo->setText(tr("&Redo") + " " + redoActionName);
            });

    auto clipController = ClipEditorViewController::instance();
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

    auto clipboardController = ClipboardController::instance();
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

    auto trackController = TracksViewController::instance();
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
    connect(actionCheckForUpdates, &QAction::triggered, this,
            [=] { Toast::show(tr("You are already up to date")); });
    auto actionAbout = new QAction(tr("About..."), this);
    connect(actionAbout, &QAction::triggered, this, [=] { Toast::show(tr("About")); });
    menuHelp->addAction(actionCheckForUpdates);
    menuHelp->addAction(actionAbout);

    addMenu(menuFile);
    addMenu(menuEdit);
    addMenu(menuInsert);
    addMenu(menuModify);
    addMenu(menuOptions);
    addMenu(menuHelp);
}
QAction *MainMenuView::actionSave() const {
    return m_actionSave;
}