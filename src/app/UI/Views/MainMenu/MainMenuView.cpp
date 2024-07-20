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
#include "UI/Window/MainWindow.h"

#include <QFileDialog>

MainMenuView::MainMenuView(MainWindow *mainWindow)
    : QMenuBar(mainWindow), m_mainWindow(mainWindow) {
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto appController = AppController::instance();
    connect(appController, &AppController::activatedPanelChanged, this,
            &MainMenuView::onActivatedPanelChanged);

    auto menuFile = new Menu(tr("&File"), this);
    auto actionNewProject = new QAction(tr("&New Project"), this);
    actionNewProject->setShortcut(QKeySequence("Ctrl+N"));
    connect(actionNewProject, &QAction::triggered, this, &MainMenuView::onNewProject);

    auto actionOpenProject = new QAction(tr("&Open Project..."));
    actionOpenProject->setShortcut(QKeySequence("Ctrl+O"));
    connect(actionOpenProject, &QAction::triggered, this, &MainMenuView::onOpenProject);

    auto actionOpenAProject = new QAction(tr("Open A Project"), this);
    connect(actionOpenAProject, &QAction::triggered, this, &MainMenuView::onOpenAProject);

    m_actionSave = new QAction(tr("&Save"), this);
    m_actionSave->setShortcut(QKeySequence("Ctrl+S"));
    m_actionSaveAs = new QAction(tr("Save &As..."), this);
    m_actionSaveAs->setShortcut(QKeySequence("Ctrl+Shift+S"));

    auto menuImport = new Menu(tr("Import"), this);
    auto actionImportMidiFile = new QAction(tr("MIDI File..."), this);
    connect(actionImportMidiFile, &QAction::triggered, this, &MainMenuView::onImportMidiFile);
    menuImport->addAction(actionImportMidiFile);

    auto menuExport = new Menu(tr("Export"), this);
    auto actionExportAudio = new QAction(tr("Audio File..."), this);
    connect(actionExportAudio, &QAction::triggered, this, &MainMenuView::onExportAudioFile);
    auto actionExportMidiFile = new QAction(tr("MIDI File..."), this);
    connect(actionExportMidiFile, &QAction::triggered, this, &MainMenuView::onExportMidiFile);

    menuExport->addAction(actionExportAudio);
    menuExport->addAction(actionExportMidiFile);

    menuFile->addAction(actionNewProject);
    menuFile->addAction(actionOpenProject);
    menuFile->addAction(actionOpenAProject);
    menuFile->addAction(m_actionSave);
    menuFile->addAction(m_actionSaveAs);
    menuFile->addSeparator();
    menuFile->addMenu(menuImport);
    menuFile->addMenu(menuExport);
    menuFile->addSeparator();

    auto menuEdit = new Menu(tr("&Edit"), this);

    auto historyManager = HistoryManager::instance();
    m_actionUndo = new QAction(tr("&Undo"), this);
    m_actionUndo->setEnabled(false);
    m_actionUndo->setShortcut(QKeySequence("Ctrl+Z"));
    connect(m_actionUndo, &QAction::triggered, historyManager, &HistoryManager::undo);

    m_actionRedo = new QAction(tr("&Redo"), this);
    m_actionRedo->setEnabled(false);
    m_actionRedo->setShortcut(QKeySequence("Ctrl+Y"));
    connect(m_actionRedo, &QAction::triggered, historyManager, &HistoryManager::redo);
    connect(historyManager, &HistoryManager::undoRedoChanged, this,
            &MainMenuView::onUndoRedoChanged);

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

    menuEdit->addAction(m_actionUndo);
    menuEdit->addAction(m_actionRedo);
    menuEdit->addSeparator();
    menuEdit->addAction(m_actionSelectAll);
    menuEdit->addAction(m_actionDelete);
    menuEdit->addSeparator();
    menuEdit->addAction(m_actionCut);
    menuEdit->addAction(m_actionCopy);
    menuEdit->addAction(m_actionPaste);

    auto menuInsert = new Menu(tr("&Insert"), this);

    auto actionInsertNewTrack = new QAction(tr("Track"), this);
    connect(actionInsertNewTrack, &QAction::triggered, TracksViewController::instance(),
            &TracksViewController::onNewTrack);
    menuInsert->addAction(actionInsertNewTrack);

    auto menuModify = new Menu(tr("&Modify"), this);
    m_actionFillLyrics = new QAction(tr("Fill Lyrics..."), this);
    m_actionFillLyrics->setShortcut(QKeySequence("Ctrl+L"));
    m_actionFillLyrics->setEnabled(false);
    auto clipController = ClipEditorViewController::instance();
    connect(m_actionFillLyrics, &QAction::triggered, clipController,
            [this] { ClipEditorViewController::instance()->onFillLyric(this); });
    menuModify->addAction(m_actionFillLyrics);

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
QAction *MainMenuView::actionSaveAs() const {
    return m_actionSaveAs;
}
void MainMenuView::onNewProject() const {
    if (!HistoryManager::instance()->isOnSavePoint()) {
        if (m_mainWindow->askSaveChanges())
            AppController::instance()->newProject();
    } else
        AppController::instance()->newProject();
}
void MainMenuView::onOpenProject() {
    auto openProject = [=] {
        auto lastDir = AppController::instance()->lastProjectFolder();
        auto fileName = QFileDialog::getOpenFileName(this, tr("Select a Project File"), lastDir,
                                                     tr("DiffScope Project File (*.dspx)"));
        if (fileName.isNull())
            return;
        AppController::instance()->openProject(fileName);
    };
    if (!HistoryManager::instance()->isOnSavePoint()) {
        if (m_mainWindow->askSaveChanges())
            openProject();
    } else
        openProject();
}
void MainMenuView::onOpenAProject() {
    auto openAProject = [=] {
        auto lastDir = AppController::instance()->lastProjectFolder();
        auto fileName = QFileDialog::getOpenFileName(this, tr("Select an A Project File"), lastDir,
                                                     tr("Project File (*.json)"));
        if (fileName.isNull())
            return;
        AppController::instance()->importAproject(fileName);
    };
    if (!HistoryManager::instance()->isOnSavePoint()) {
        if (m_mainWindow->askSaveChanges())
            openAProject();
    } else
        openAProject();
}
void MainMenuView::onImportMidiFile() {
    auto fileName =
        QFileDialog::getOpenFileName(this, tr("Select a MIDI File"), ".", tr("MIDI File (*.mid)"));
    if (fileName.isNull())
        return;
    AppController::instance()->importMidiFile(fileName);
}
void MainMenuView::onExportMidiFile() {
    auto fileName =
        QFileDialog::getSaveFileName(this, tr("Save as MIDI File"), ".", tr("MIDI File (*.mid)"));
    if (fileName.isNull())
        return;
    AppController::instance()->exportMidiFile(fileName);
}
void MainMenuView::onExportAudioFile() {
    AudioExportDialog dlg(this);
    dlg.exec();
}
void MainMenuView::onUndoRedoChanged(bool canUndo, const QString &undoName, bool canRedo,
                                     const QString &redoName) {
    m_actionUndo->setEnabled(canUndo);
    m_actionUndo->setText(tr("&Undo") + " " + undoName);
    m_actionRedo->setEnabled(canRedo);
    m_actionRedo->setText(tr("&Redo") + " " + redoName);
}
void MainMenuView::onActivatedPanelChanged(AppGlobal::PanelType panel) {
    m_panelType = panel;
    auto clipController = ClipEditorViewController::instance();
    if (panel == AppGlobal::ClipEditor) {
        m_actionSelectAll->setEnabled(clipController->canSelectAll());
        connect(clipController, &ClipEditorViewController::canSelectAllChanged, m_actionSelectAll,
                &QAction::setEnabled);

        m_actionDelete->setEnabled(clipController->hasSelectedNotes());
        connect(clipController, &ClipEditorViewController::hasSelectedNotesChanged, m_actionDelete,
                &QAction::setEnabled);

        m_actionFillLyrics->setEnabled(clipController->hasSelectedNotes());
        connect(clipController, &ClipEditorViewController::hasSelectedNotesChanged,
                m_actionFillLyrics, &QAction::setEnabled);
    } else {
        disconnect(clipController, &ClipEditorViewController::canSelectAllChanged,
                   m_actionSelectAll, &QAction::setEnabled);
        m_actionSelectAll->setEnabled(false);

        disconnect(clipController, &ClipEditorViewController::hasSelectedNotesChanged,
                   m_actionDelete, &QAction::setEnabled);
        m_actionDelete->setEnabled(false);

        disconnect(clipController, &ClipEditorViewController::hasSelectedNotesChanged,
                   m_actionFillLyrics, &QAction::setEnabled);
        m_actionFillLyrics->setEnabled(false);
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