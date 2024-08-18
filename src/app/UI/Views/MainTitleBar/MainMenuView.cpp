//
// Created by fluty on 2024/7/13.
//

#include "MainMenuView.h"

#include "MainMenuView_p.h"
#include "Controller/AppController.h"
#include "Controller/ClipEditorViewController.h"
#include "Controller/TracksViewController.h"
#include "Modules/History/HistoryManager.h"
#include "UI/Controls/Toast.h"
#include "UI/Dialogs/Audio/AudioExportDialog.h"
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
    auto actionNewProject = new QAction(tr("&New Project"), this);
    actionNewProject->setShortcut(QKeySequence("Ctrl+N"));
    connect(actionNewProject, &QAction::triggered, this, [=] { d->onNewProject(); });

    auto actionOpenProject = new QAction(tr("&Open Project..."));
    actionOpenProject->setShortcut(QKeySequence("Ctrl+O"));
    connect(actionOpenProject, &QAction::triggered, this, [=] { d->onOpenProject(); });

    auto actionOpenAProject = new QAction(tr("Open A Project"), this);
    connect(actionOpenAProject, &QAction::triggered, this, [=] { d->onOpenAProject(); });

    d->m_actionSave = new QAction(tr("&Save"), this);
    d->m_actionSave->setShortcut(QKeySequence("Ctrl+S"));
    d->m_actionSaveAs = new QAction(tr("Save &As..."), this);
    d->m_actionSaveAs->setShortcut(QKeySequence("Ctrl+Shift+S"));

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

    menuFile->addAction(actionNewProject);
    menuFile->addAction(actionOpenProject);
    menuFile->addAction(actionOpenAProject);
    menuFile->addAction(d->m_actionSave);
    menuFile->addAction(d->m_actionSaveAs);
    menuFile->addSeparator();
    menuFile->addMenu(menuImport);
    menuFile->addMenu(menuExport);
    menuFile->addSeparator();
    menuFile->addAction(actionExit);

    auto menuEdit = new CMenu(tr("&Edit"), this);

    d->m_actionUndo = new QAction(tr("&Undo"), this);
    d->m_actionUndo->setEnabled(false);
    d->m_actionUndo->setShortcut(QKeySequence("Ctrl+Z"));
    connect(d->m_actionUndo, &QAction::triggered, historyManager, &HistoryManager::undo);

    d->m_actionRedo = new QAction(tr("&Redo"), this);
    d->m_actionRedo->setEnabled(false);
    d->m_actionRedo->setShortcut(QKeySequence("Ctrl+Y"));
    connect(d->m_actionRedo, &QAction::triggered, historyManager, &HistoryManager::redo);
    connect(historyManager, &HistoryManager::undoRedoChanged, this,
            [=](bool canUndo, const QString &undoName, bool canRedo, const QString &redoName) {
                d->onUndoRedoChanged(canUndo, undoName, canRedo, redoName);
            });

    d->m_actionSelectAll = new QAction(tr("Select &All"), this);
    d->m_actionSelectAll->setShortcut(QKeySequence("Ctrl+A"));
    d->m_actionSelectAll->setEnabled(false);
    connect(d->m_actionSelectAll, &QAction::triggered, this, [=] { d->onSelectAll(); });

    d->m_actionDelete = new QAction(tr("&Delete"), this);
    d->m_actionDelete->setShortcut(Qt::Key_Delete);
    d->m_actionDelete->setEnabled(false);
    connect(d->m_actionDelete, &QAction::triggered, this, [=] { d->onDelete(); });

    d->m_actionCut = new QAction(tr("Cu&t"), this);
    d->m_actionCut->setShortcut(QKeySequence("Ctrl+X"));
    connect(d->m_actionCut, &QAction::triggered, this, [=] { d->onCut(); });

    d->m_actionCopy = new QAction(tr("&Copy"), this);
    d->m_actionCopy->setShortcut(QKeySequence("Ctrl+C"));
    connect(d->m_actionCopy, &QAction::triggered, this, [=] { d->onCopy(); });

    d->m_actionPaste = new QAction(tr("&Paste"), this);
    d->m_actionPaste->setShortcut(QKeySequence("Ctrl+V"));
    connect(d->m_actionPaste, &QAction::triggered, this, [=] { d->onPaste(); });

    menuEdit->addAction(d->m_actionUndo);
    menuEdit->addAction(d->m_actionRedo);
    menuEdit->addSeparator();
    menuEdit->addAction(d->m_actionSelectAll);
    menuEdit->addAction(d->m_actionDelete);
    menuEdit->addSeparator();
    menuEdit->addAction(d->m_actionCut);
    menuEdit->addAction(d->m_actionCopy);
    menuEdit->addAction(d->m_actionPaste);

    // auto menuInsert = new CMenu(tr("&Insert"), this);
    //
    // auto actionInsertNewTrack = new QAction(tr("Track"), this);
    // connect(actionInsertNewTrack, &QAction::triggered, trackController,
    //         &TracksViewController::onNewTrack);
    // menuInsert->addAction(actionInsertNewTrack);

    // auto menuModify = new CMenu(tr("&Modify"), this);
    d->m_actionFillLyrics = new QAction(tr("Fill Lyrics..."), this);
    d->m_actionFillLyrics->setShortcut(QKeySequence("Ctrl+L"));
    d->m_actionFillLyrics->setEnabled(false);
    connect(d->m_actionFillLyrics, &QAction::triggered, clipController,
            [this] { clipController->onFillLyric(this); });
    // menuModify->addAction(d->m_actionFillLyrics);
    menuEdit->addSeparator();
    menuEdit->addAction(d->m_actionFillLyrics);

    auto menuOptions = new CMenu(tr("&Options"), this);
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
    auto actionMidiSettings = new QAction(tr("&MIDI..."), this);
    connect(actionMidiSettings, &QAction::triggered, this, [=] {
        AppOptionsDialog dialog(AppOptionsDialog::Midi, this);
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
    menuOptions->addAction(actionMidiSettings);
    menuOptions->addAction(actionAppearanceOptions);
    menuOptions->addAction(actionLanguage);

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
    return d->m_actionSave;
}

QAction *MainMenuView::actionSaveAs() {
    Q_D(MainMenuView);
    return d->m_actionSaveAs;
}

void MainMenuViewPrivate::onNewProject() const {
    if (!historyManager->isOnSavePoint()) {
        if (m_mainWindow->askSaveChanges())
            appController->newProject();
    } else
        appController->newProject();
}

void MainMenuViewPrivate::onOpenProject() {
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

void MainMenuViewPrivate::onOpenAProject() {
    Q_Q(MainMenuView);
    auto openAProject = [=] {
        auto lastDir = appController->lastProjectFolder();
        auto fileName = QFileDialog::getOpenFileName(q, tr("Select an A Project File"), lastDir,
                                                     tr("Project File (*.json)"));
        if (fileName.isNull())
            return;
        appController->importAproject(fileName);
    };
    if (!historyManager->isOnSavePoint()) {
        if (m_mainWindow->askSaveChanges())
            openAProject();
    } else
        openAProject();
}

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
    m_actionUndo->setEnabled(canUndo);
    m_actionUndo->setText(tr("&Undo") + " " + undoName);
    m_actionRedo->setEnabled(canRedo);
    m_actionRedo->setText(tr("&Redo") + " " + redoName);
}

void MainMenuViewPrivate::onActivatedPanelChanged(AppGlobal::PanelType panel) {
    Q_Q(MainMenuView);
    m_panelType = panel;
    if (panel == AppGlobal::ClipEditor) {
        m_actionSelectAll->setEnabled(clipController->canSelectAll());
        QObject::connect(clipController, &ClipEditorViewController::canSelectAllChanged,
                         m_actionSelectAll, &QAction::setEnabled);

        m_actionDelete->setEnabled(clipController->hasSelectedNotes());
        QObject::connect(clipController, &ClipEditorViewController::hasSelectedNotesChanged,
                         m_actionDelete, &QAction::setEnabled);

        m_actionFillLyrics->setEnabled(clipController->hasSelectedNotes());
        QObject::connect(clipController, &ClipEditorViewController::hasSelectedNotesChanged,
                         m_actionFillLyrics, &QAction::setEnabled);
    } else {
        QObject::disconnect(clipController, &ClipEditorViewController::canSelectAllChanged,
                            m_actionSelectAll, &QAction::setEnabled);
        m_actionSelectAll->setEnabled(false);

        QObject::disconnect(clipController, &ClipEditorViewController::hasSelectedNotesChanged,
                            m_actionDelete, &QAction::setEnabled);
        m_actionDelete->setEnabled(false);

        QObject::disconnect(clipController, &ClipEditorViewController::hasSelectedNotesChanged,
                            m_actionFillLyrics, &QAction::setEnabled);
        m_actionFillLyrics->setEnabled(false);
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

void MainMenuViewPrivate::exitApp() {
    qDebug() << "MainMenuViewPrivate::exitApp";
    m_mainWindow->close();
}