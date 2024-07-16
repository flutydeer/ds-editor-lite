//
// Created by fluty on 2024/7/13.
//

#include "MainMenuView.h"

#include "Controller/AppController.h"
#include "Controller/ClipEditorViewController.h"
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

    connect(appController, &AppController::activatedPanelChanged, this,
            &MainMenuView::onActivatedPanelChanged);

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

    m_actionSelectAll = new QAction(tr("Select &All"), this);
    m_actionSelectAll->setShortcut(QKeySequence("Ctrl+A"));
    m_actionSelectAll->setEnabled(false);
    connect(m_actionSelectAll, &QAction::triggered, this, &MainMenuView::onSelectAll);

    m_actionDelete = new QAction(tr("&Delete"), this);
    m_actionDelete->setShortcut(Qt::Key_Delete);
    m_actionDelete->setEnabled(false);
    connect(m_actionDelete, &QAction::triggered, this, &MainMenuView::onDelete);

    m_actionCut = new QAction(tr("Cu&t"), this);
    m_actionCut->setShortcut(QKeySequence("Ctrl+X"));
    connect(m_actionCut, &QAction::triggered, this, &MainMenuView::onCut);

    m_actionCopy = new QAction(tr("&Copy"), this);
    m_actionCopy->setShortcut(QKeySequence("Ctrl+C"));
    connect(m_actionCopy, &QAction::triggered, this, &MainMenuView::onCopy);

    m_actionPaste = new QAction(tr("&Paste"), this);
    m_actionPaste->setShortcut(QKeySequence("Ctrl+V"));
    connect(m_actionPaste, &QAction::triggered, this, &MainMenuView::onPaste);

    menuEdit->addAction(actionUndo);
    menuEdit->addAction(actionRedo);
    menuEdit->addSeparator();
    menuEdit->addAction(m_actionSelectAll);
    menuEdit->addAction(m_actionDelete);
    menuEdit->addSeparator();
    menuEdit->addAction(m_actionCut);
    menuEdit->addAction(m_actionCopy);
    menuEdit->addAction(m_actionPaste);

    auto menuInsert = new Menu(tr("&Insert"), this);

    auto trackController = TracksViewController::instance();
    auto actionInsertNewTrack = new QAction(tr("Track"), this);
    connect(actionInsertNewTrack, &QAction::triggered, TracksViewController::instance(),
            &TracksViewController::onNewTrack);
    menuInsert->addAction(actionInsertNewTrack);

    // TODO: 在没有选中音符之前禁用填入歌词
    auto menuModify = new Menu(tr("&Modify"), this);
    auto actionFillLyrics = new QAction(tr("Fill Lyrics..."), this);
    actionFillLyrics->setShortcut(QKeySequence("Ctrl+L"));
    auto clipController = ClipEditorViewController::instance();
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
void MainMenuView::onActivatedPanelChanged(AppGlobal::PanelType panel) {
    m_panelType = panel;
    auto clipController = ClipEditorViewController::instance();
    if (panel == AppGlobal::ClipEditor) {
        m_actionSelectAll->setEnabled(clipController->canSelectAll());
        connect(clipController, &ClipEditorViewController::canSelectAllChanged, m_actionSelectAll,
                &QAction::setEnabled);
        m_actionDelete->setEnabled(clipController->canDelete());
        connect(clipController, &ClipEditorViewController::canDeleteChanged, m_actionDelete,
                &QAction::setEnabled);
    } else {
        disconnect(clipController, &ClipEditorViewController::canSelectAllChanged,
                   m_actionSelectAll, &QAction::setEnabled);
        m_actionSelectAll->setEnabled(clipController->canDelete());
        disconnect(clipController, &ClipEditorViewController::canDeleteChanged, m_actionDelete,
                   &QAction::setEnabled);
        m_actionDelete->setEnabled(false);
    }
}
void MainMenuView::onSelectAll() {
    qDebug() << "MainMenuView::onSelectAll";
    if (m_panelType == AppGlobal::ClipEditor)
        ClipEditorViewController::instance()->onSelectAllNotes();
}
void MainMenuView::onDelete() {
    qDebug() << "MainMenuView::onDelete";
    if (m_panelType == AppGlobal::ClipEditor)
        ClipEditorViewController::instance()->onDeleteSelectedNotes();
}
void MainMenuView::onCut() {
    qDebug() << "MainMenuView::onCut";
}
void MainMenuView::onCopy() {
    qDebug() << "MainMenuView::onCopy";
}
void MainMenuView::onPaste() {
    qDebug() << "MainMenuView::onPaste";
}