//
// Created by fluty on 2024/7/13.
//

#include "MainMenuView.h"

#include "MainMenuView_p.h"
#include "Controller/AppController.h"
#include "Controller/ClipboardController.h"
#include "Controller/ClipController.h"
#include "Controller/TrackController.h"
#include "Global/ControllerGlobal.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/AudioClip.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppStatus/AppStatus.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>
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
#include "UI/Utils/IconUtils.h"
#include "UI/Dialogs/Help/DiscoverDiffScopeDialog.h"

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>

#include <algorithm>

#include "UI/Controls/Menu.h"

namespace {

    constexpr QSize kMenuIconSize(16, 16);
    const QColor kMenuIconColor(0xE0, 0xE0, 0xE0);
    const QColor kMenuIconDisabledColor(0x90, 0x90, 0x90);

    QIcon menuIcon(const QString &path, const QSize &size = kMenuIconSize) {
        return IconUtils::createTintedSvgIcon(path, size, kMenuIconColor, kMenuIconDisabledColor);
    }

}

MainMenuView::MainMenuView(MainWindow *mainWindow)
    : QMenuBar(mainWindow), d_ptr(new MainMenuViewPrivate(mainWindow)) {
    Q_D(MainMenuView);
    d->q_ptr = this;

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    connect(appController, &AppController::activePanelChanged, this,
            [=](AppGlobal::PanelType panel) { d->onActivatedPanelChanged(panel); });

    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this,
            [this] { d_ptr->updatePasteActionState(); });

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

QAction *MainMenuView::actionNew() {
    Q_D(MainMenuView);
    return d->actionNew;
}

QAction *MainMenuView::actionOpen() {
    Q_D(MainMenuView);
    return d->actionOpen;
}

QAction *MainMenuView::actionSaveAs() {
    Q_D(MainMenuView);
    return d->actionSaveAs;
}

void MainMenuView::openRecentProject(const QString &filePath) {
    Q_D(MainMenuView);
    d->onOpenRecentProject(filePath);
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
    const auto lastDir = appController->lastProjectFolder();
    const auto fileName = QFileDialog::getOpenFileName(
        q, tr("Open"), lastDir,
        MainMenuView::tr("All Supported Files (*.dspx *.mid *.midi);;DiffScope Project File "
                         "(*.dspx);;MIDI File (*.mid *.midi)"));
    if (fileName.isNull()) {
        qDebug() << "User cancelled open";
        return;
    }
    openFileWithSavePrompt(fileName);
}

void MainMenuViewPrivate::onOpenRecentProject(const QString &filePath) {
    if (!QFile::exists(filePath)) {
        appController->removeRecentProjectFile(filePath);
        Toast::show(tr("File does not exist: %1").arg(filePath));
        return;
    }
    openFileWithSavePrompt(filePath);
}

void MainMenuViewPrivate::onClearRecentProjects() {
    appController->clearRecentProjectFiles();
}

void MainMenuViewPrivate::refreshRecentProjectsMenu() {
    if (!menuRecentProjects)
        return;

    menuRecentProjects->clear();
    const auto files = appController->recentProjectFiles();
    if (files.isEmpty()) {
        const auto actionEmpty = menuRecentProjects->addAction(tr("(No Recent Projects)"));
        actionEmpty->setEnabled(false);
        menuRecentProjects->addSeparator();
        actionClearRecentProjects->setEnabled(false);
        menuRecentProjects->addAction(actionClearRecentProjects);
        return;
    }

    const auto count = std::min(files.size(), qsizetype{10});
    for (qsizetype i = 0; i < count; ++i) {
        const auto filePath = files.at(i);
        const auto text =
            tr("&%1 %2").arg(static_cast<int>(i + 1)).arg(QFileInfo(filePath).fileName());
        const auto action = menuRecentProjects->addAction(text);
        action->setData(filePath);
        action->setToolTip(filePath);
        action->setStatusTip(filePath);
        connect(action, &QAction::triggered, this,
                [this, filePath] { onOpenRecentProject(filePath); });
    }
    menuRecentProjects->addSeparator();
    actionClearRecentProjects->setEnabled(true);
    menuRecentProjects->addAction(actionClearRecentProjects);
}

void MainMenuViewPrivate::openFileWithSavePrompt(const QString &filePath) {
    auto openFile = [=] {
        QString errorMessage;
        appController->openFile(filePath, errorMessage);
    };
    if (!historyManager->isOnSavePoint()) {
        if (m_mainWindow->askSaveChanges())
            openFile();
    } else
        openFile();
}

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
        exitTracksEditorState();
        enterClipEditorState();
    } else if (panel == AppGlobal::TracksEditor) {
        exitClipEditorState();
        enterTracksEditorState();
    } else {
        exitClipEditorState();
        exitTracksEditorState();
    }
}

void MainMenuViewPrivate::onSelectAll() {
    Q_Q(MainMenuView);
    qDebug() << "MainMenuView::onSelectAll";
    if (m_panelType == AppGlobal::ClipEditor)
        clipController->onSelectAllNotes();
    else if (m_panelType == AppGlobal::TracksEditor)
        qDebug() << "MainMenuView::onSelectAll: not implemented for TracksEditor";
}

void MainMenuViewPrivate::onDelete() {
    Q_Q(MainMenuView);
    qDebug() << "MainMenuView::onDelete";
    if (m_panelType == AppGlobal::ClipEditor)
        clipController->onDeleteSelectedNotes();
    else if (m_panelType == AppGlobal::TracksEditor)
        trackController->onRemoveClips(appStatus->selectedClips.get());
}

void MainMenuViewPrivate::onCut() {
    if (m_panelType == AppGlobal::ClipEditor)
        clipboardController->cut();
    else if (m_panelType == AppGlobal::TracksEditor)
        trackController->cutSelectedClips();
}

void MainMenuViewPrivate::onCopy() {
    if (m_panelType == AppGlobal::ClipEditor)
        clipboardController->copy();
    else if (m_panelType == AppGlobal::TracksEditor)
        trackController->copySelectedClips();
}

void MainMenuViewPrivate::onPaste() {
    if (m_panelType == AppGlobal::ClipEditor)
        clipboardController->paste();
    else if (m_panelType == AppGlobal::TracksEditor)
        clipboardController->paste();
}

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

    actionCut->setEnabled(hasSelectedNotes);
    QObject::connect(clipController, &ClipController::hasSelectedNotesChanged, actionCut,
                     &QAction::setEnabled);

    actionCopy->setEnabled(hasSelectedNotes);
    QObject::connect(clipController, &ClipController::hasSelectedNotesChanged, actionCopy,
                     &QAction::setEnabled);

    updatePasteActionState();

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

    QObject::disconnect(clipController, &ClipController::hasSelectedNotesChanged, actionCut,
                        &QAction::setEnabled);
    actionCut->setEnabled(false);

    QObject::disconnect(clipController, &ClipController::hasSelectedNotesChanged, actionCopy,
                        &QAction::setEnabled);
    actionCopy->setEnabled(false);

    actionPaste->setEnabled(false);

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

void MainMenuViewPrivate::enterTracksEditorState() {
    Q_Q(MainMenuView);

    const auto hasSelectedClips = !appStatus->selectedClips.get().isEmpty();
    actionCut->setEnabled(hasSelectedClips);
    actionCopy->setEnabled(hasSelectedClips);
    actionDelete->setEnabled(hasSelectedClips);
    updatePasteActionState();
    actionSelectAll->setEnabled(true);

    connect(appStatus, &AppStatus::clipSelectionChanged, q, [this](const QList<int> &clips) {
        const auto hasSelection = !clips.isEmpty();
        actionCut->setEnabled(hasSelection);
        actionCopy->setEnabled(hasSelection);
        actionDelete->setEnabled(hasSelection);
    });

    actionOctaveUp->setEnabled(false);
    actionOctaveDown->setEnabled(false);
    actionFillLyrics->setEnabled(false);
}

void MainMenuViewPrivate::exitTracksEditorState() {
    disconnect(appStatus, &AppStatus::clipSelectionChanged, nullptr, nullptr);

    actionCut->setEnabled(false);
    actionCopy->setEnabled(false);
    actionDelete->setEnabled(false);
    actionPaste->setEnabled(false);
    actionSelectAll->setEnabled(false);
}

void MainMenuViewPrivate::updatePasteActionState() {
    const auto mimeData = QGuiApplication::clipboard()->mimeData();
    if (!mimeData) {
        actionPaste->setEnabled(false);
        return;
    }
    if (m_panelType == AppGlobal::ClipEditor)
        actionPaste->setEnabled(mimeData->hasFormat(
            ControllerGlobal::ElemMimeType.at(ControllerGlobal::NoteWithParams)));
    else if (m_panelType == AppGlobal::TracksEditor)
        actionPaste->setEnabled(
            mimeData->hasFormat(ControllerGlobal::ElemMimeType.at(ControllerGlobal::Clip)));
    else
        actionPaste->setEnabled(false);
}

void MainMenuViewPrivate::initActions() {
    initFileActions();
    initEditActions();
}

void MainMenuViewPrivate::initFileActions() {
    actionNew = new QAction(tr("&New"), this);
    actionNew->setIcon(menuIcon(QStringLiteral(":/svg/icons/document_add_16_regular.svg")));
    actionNew->setShortcut(QKeySequence("Ctrl+N"));
    actionNew->setShortcutContext(Qt::ApplicationShortcut);
    connect(actionNew, &QAction::triggered, this, [this] { onNew(); });

    actionOpen = new QAction(tr("&Open..."));
    actionOpen->setIcon(menuIcon(QStringLiteral(":/svg/icons/open_16_regular.svg")));
    actionOpen->setShortcut(QKeySequence("Ctrl+O"));
    actionOpen->setShortcutContext(Qt::ApplicationShortcut);
    connect(actionOpen, &QAction::triggered, this, [this] { onOpen(); });

    actionClearRecentProjects = new QAction(tr("Clear Recent Projects"), this);
    actionClearRecentProjects->setIcon(
        menuIcon(QStringLiteral(":/svg/icons/delete_16_regular.svg")));
    connect(actionClearRecentProjects, &QAction::triggered, this,
            [this] { onClearRecentProjects(); });

    actionSave = new QAction(tr("&Save"), this);
    actionSave->setIcon(menuIcon(QStringLiteral(":/svg/icons/save_16_regular.svg")));
    actionSave->setShortcut(QKeySequence("Ctrl+S"));
    actionSave->setShortcutContext(Qt::ApplicationShortcut);

    actionSaveAs = new QAction(tr("Save &as..."), this);
    actionSaveAs->setIcon(
        menuIcon(QStringLiteral(":/svg/icons/save_edit_20_regular.svg"), QSize(20, 20)));
    actionSaveAs->setShortcut(QKeySequence("Ctrl+Shift+S"));
    actionSaveAs->setShortcutContext(Qt::ApplicationShortcut);

    actionImportMidi = new QAction(tr("MIDI file..."), this);
    actionImportMidi->setIcon(menuIcon(QStringLiteral(":/svg/icons/arrow_import_16_regular.svg")));
    connect(actionImportMidi, &QAction::triggered, this, [this] { onImportMidiFile(); });

    actionExportAudio = new QAction(tr("Audio file..."), this);
    actionExportAudio->setIcon(menuIcon(QStringLiteral(":/svg/icons/arrow_export_16_regular.svg")));
    connect(actionExportAudio, &QAction::triggered, this, [this] { onExportAudioFile(); });

    actionExportMidi = new QAction(tr("MIDI file..."), this);
    actionExportMidi->setIcon(menuIcon(QStringLiteral(":/svg/icons/arrow_export_16_regular.svg")));
    connect(actionExportMidi, &QAction::triggered, this, [this] { onExportMidiFile(); });

    actionOpenPackageManager = new QAction(tr("Manage packages..."), this);
    actionOpenPackageManager->setIcon(
        menuIcon(QStringLiteral(":/svg/icons/settings_16_regular.svg")));
    connect(actionOpenPackageManager, &QAction::triggered, this, [] {
        PackageManagerDialog dialog;
        dialog.exec();
    });

    actionExit = new QAction(tr("E&xit"), this);
    actionExit->setIcon(menuIcon(QStringLiteral(":/svg/icons/dismiss_16_regular.svg")));
    connect(actionExit, &QAction::triggered, this, [this] { exitApp(); });
}

void MainMenuViewPrivate::initEditActions() {
    Q_Q(MainMenuView);
    actionUndo = new QAction(tr("&Undo"), this);
    actionUndo->setIcon(menuIcon(QStringLiteral(":/svg/icons/arrow_undo_16_regular.svg")));
    actionUndo->setEnabled(false);
    actionUndo->setShortcut(QKeySequence("Ctrl+Z"));
    actionUndo->setShortcutContext(Qt::ApplicationShortcut);
    connect(actionUndo, &QAction::triggered, historyManager, &HistoryManager::undo);

    actionRedo = new QAction(tr("&Redo"), this);
    actionRedo->setIcon(menuIcon(QStringLiteral(":/svg/icons/arrow_redo_16_regular.svg")));
    actionRedo->setEnabled(false);
    actionRedo->setShortcut(QKeySequence("Ctrl+Y"));
    actionRedo->setShortcutContext(Qt::ApplicationShortcut);
    connect(actionRedo, &QAction::triggered, historyManager, &HistoryManager::redo);
    connect(historyManager, &HistoryManager::undoRedoChanged, this,
            [this](bool canUndo, const QString &undoName, bool canRedo, const QString &redoName) {
                onUndoRedoChanged(canUndo, undoName, canRedo, redoName);
            });

    actionSelectAll = new QAction(tr("Select &all"), this);
    actionSelectAll->setIcon(menuIcon(QStringLiteral(":/svg/icons/select_all_on_16_regular.svg")));
    actionSelectAll->setShortcut(QKeySequence("Ctrl+A"));
    actionSelectAll->setShortcutContext(Qt::ApplicationShortcut);
    actionSelectAll->setEnabled(false);
    connect(actionSelectAll, &QAction::triggered, this, [this] { onSelectAll(); });

    actionDelete = new QAction(tr("&Delete"), this);
    actionDelete->setIcon(menuIcon(QStringLiteral(":/svg/icons/delete_16_regular.svg")));
    actionDelete->setShortcut(Qt::Key_Delete);
    actionDelete->setShortcutContext(Qt::ApplicationShortcut);
    actionDelete->setEnabled(false);
    connect(actionDelete, &QAction::triggered, this, [this] { onDelete(); });

    actionCut = new QAction(tr("Cu&t"), this);
    actionCut->setIcon(menuIcon(QStringLiteral(":/svg/icons/cut_16_regular.svg")));
    actionCut->setShortcut(QKeySequence("Ctrl+X"));
    actionCut->setShortcutContext(Qt::ApplicationShortcut);
    connect(actionCut, &QAction::triggered, this, [this] { onCut(); });

    actionCopy = new QAction(tr("&Copy"), this);
    actionCopy->setIcon(menuIcon(QStringLiteral(":/svg/icons/copy_16_regular.svg")));
    actionCopy->setShortcut(QKeySequence("Ctrl+C"));
    actionCopy->setShortcutContext(Qt::ApplicationShortcut);
    connect(actionCopy, &QAction::triggered, this, [this] { onCopy(); });

    actionPaste = new QAction(tr("&Paste"), this);
    actionPaste->setIcon(menuIcon(QStringLiteral(":/svg/icons/clipboard_paste_16_regular.svg")));
    actionPaste->setShortcut(QKeySequence("Ctrl+V"));
    actionPaste->setShortcutContext(Qt::ApplicationShortcut);
    connect(actionPaste, &QAction::triggered, this, [this] { onPaste(); });

    actionOctaveUp = new QAction(tr("Move an octave up"), this);
    actionOctaveUp->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Up));
    actionOctaveUp->setShortcutContext(Qt::ApplicationShortcut);
    connect(actionOctaveUp, &QAction::triggered, this, [this] { onOctaveUp(); });

    actionOctaveDown = new QAction(tr("Move an octave down"), this);
    actionOctaveDown->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Down));
    actionOctaveDown->setShortcutContext(Qt::ApplicationShortcut);
    connect(actionOctaveDown, &QAction::triggered, this, [this] { onOctaveDown(); });

    actionFillLyrics = new QAction(tr("Fill lyrics..."), this);
    // TODO: 恢复工程文件图标后再补回这里的菜单图标。
    actionFillLyrics->setShortcut(QKeySequence("Ctrl+L"));
    actionFillLyrics->setShortcutContext(Qt::ApplicationShortcut);
    actionFillLyrics->setEnabled(false);
    connect(actionFillLyrics, &QAction::triggered, clipController,
            [this, q] { clipController->onFillLyric(q); });

    actionSearchLyrics = new QAction(tr("Search lyrics..."), this);
    actionSearchLyrics->setIcon(menuIcon(QStringLiteral(":/svg/icons/search_16_regular.svg")));
    actionSearchLyrics->setShortcut(QKeySequence("Ctrl+F"));
    actionSearchLyrics->setShortcutContext(Qt::ApplicationShortcut);
    connect(actionSearchLyrics, &QAction::triggered, clipController,
            [this, q] { clipController->onSearchLyric(q); });

    actionExtractPitchParam = new QAction(tr("Extract pitch parameter..."));
    connect(actionExtractPitchParam, &QAction::triggered, this, [this] { onExtractPitchParam(); });
}

Menu *MainMenuViewPrivate::buildFileMenu() {
    Q_Q(MainMenuView);
    auto menuFile = new Menu(tr("&File"), q);
    menuFile->addAction(actionNew);
    menuFile->addAction(actionOpen);

    menuRecentProjects = new Menu(tr("Recent Projects"), q);
    menuRecentProjects->menuAction()->setIcon(
        menuIcon(QStringLiteral(":/svg/icons/history_16_regular.svg")));
    connect(menuRecentProjects, &Menu::aboutToShow, this, [this] { refreshRecentProjectsMenu(); });
    connect(appController, &AppController::recentProjectFilesChanged, this,
            [this] { refreshRecentProjectsMenu(); });
    refreshRecentProjectsMenu();
    menuFile->addMenu(menuRecentProjects);

    menuFile->addAction(actionSave);
    menuFile->addAction(actionSaveAs);

    menuFile->addSeparator();

    auto menuImport = new Menu(tr("Import"), q);
    menuImport->menuAction()->setIcon(
        menuIcon(QStringLiteral(":/svg/icons/arrow_import_16_regular.svg")));
    menuImport->addAction(actionImportMidi);
    menuFile->addMenu(menuImport);

    auto menuExport = new Menu(tr("Export"), q);
    menuExport->menuAction()->setIcon(
        menuIcon(QStringLiteral(":/svg/icons/arrow_export_16_regular.svg")));
    menuExport->addAction(actionExportAudio);
    menuExport->addAction(actionExportMidi);
    menuFile->addMenu(menuExport);

    menuFile->addSeparator();

    menuFile->addAction(actionOpenPackageManager);
    menuFile->addSeparator();

    menuFile->addAction(actionExit);
    return menuFile;
}

Menu *MainMenuViewPrivate::buildEditMenu() {
    Q_Q(MainMenuView);
    auto menuEdit = new Menu(tr("&Edit"), q);
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

Menu *MainMenuViewPrivate::buildOptionsMenu() {
    Q_Q(MainMenuView);
    auto actionGeneralOptions = new QAction(tr("&General..."), this);
    actionGeneralOptions->setIcon(menuIcon(QStringLiteral(":/svg/icons/settings_16_regular.svg")));
    connect(actionGeneralOptions, &QAction::triggered, this, [] {
        AppOptionsDialog dialog(AppOptionsGlobal::Option::General);
        dialog.exec();
    });
    auto actionAudioSettings = new QAction(tr("&Audio..."), this);
    actionAudioSettings->setIcon(menuIcon(QStringLiteral(":/svg/icons/settings_16_regular.svg")));
    connect(actionAudioSettings, &QAction::triggered, this, [] {
        AppOptionsDialog dialog(AppOptionsGlobal::Option::Audio);
        dialog.exec();
    });
    auto actionMidiSettings = new QAction(tr("&MIDI..."), this);
    actionMidiSettings->setIcon(menuIcon(QStringLiteral(":/svg/icons/settings_16_regular.svg")));
    connect(actionMidiSettings, &QAction::triggered, this, [] {
        AppOptionsDialog dialog(AppOptionsGlobal::Option::Midi);
        dialog.exec();
    });
    auto actionAppearanceOptions = new QAction(tr("A&ppearance..."), this);
    actionAppearanceOptions->setIcon(
        menuIcon(QStringLiteral(":/svg/icons/settings_16_regular.svg")));
    connect(actionAppearanceOptions, &QAction::triggered, this, [] {
        AppOptionsDialog dialog(AppOptionsGlobal::Option::Appearance);
        dialog.exec();
    });
    // const auto actionLanguageOptions = new QAction(tr("&Language..."), this);
    // connect(actionLanguageOptions, &QAction::triggered, this, [] {
    //     AppOptionsDialog dialog(AppOptionsGlobal::Option::Language);
    //     dialog.exec();
    // });
    const auto actionInferenceOptions = new QAction(tr("&Inference..."), this);
    actionInferenceOptions->setIcon(
        menuIcon(QStringLiteral(":/svg/icons/settings_16_regular.svg")));
    connect(actionInferenceOptions, &QAction::triggered, this, [] {
        AppOptionsDialog dialog(AppOptionsGlobal::Option::Inference);
        dialog.exec();
    });
    const auto actionDeveloperOptions = new QAction(tr("&Developer Options..."), this);
    actionDeveloperOptions->setIcon(
        menuIcon(QStringLiteral(":/svg/icons/settings_16_regular.svg")));
    connect(actionDeveloperOptions, &QAction::triggered, this, [] {
        AppOptionsDialog dialog(AppOptionsGlobal::Option::DeveloperOptions);
        dialog.exec();
    });

    auto menuOptions = new Menu(tr("&Options"), q);
    menuOptions->addAction(actionGeneralOptions);
    menuOptions->addAction(actionAudioSettings);
    menuOptions->addAction(actionMidiSettings);
    menuOptions->addAction(actionAppearanceOptions);
    // menuOptions->addAction(actionLanguageOptions);
    menuOptions->addAction(actionInferenceOptions);
    menuOptions->addSeparator();
    menuOptions->addAction(actionDeveloperOptions);
    return menuOptions;
}

Menu *MainMenuViewPrivate::buildHelpMenu() {
    Q_Q(MainMenuView);
    auto actionCheckForUpdates = new QAction(tr("Check for updates"), this);
    actionCheckForUpdates->setIcon(menuIcon(QStringLiteral(":/svg/icons/info_16_regular.svg")));
    connect(actionCheckForUpdates, &QAction::triggered, this,
            [=] { Toast::show(tr("You are already up to date")); });
    auto actionAbout = new QAction(tr("About..."), this);
    actionAbout->setIcon(menuIcon(QStringLiteral(":/svg/icons/info_16_regular.svg")));
    connect(actionAbout, &QAction::triggered, this, [] { Toast::show(tr("About")); });

    auto actionDiscoverDiffScope = new QAction(tr("Discover DiffScope"), this);
    connect(actionDiscoverDiffScope, &QAction::triggered, this, [] {
        DiscoverDiffScopeDialog dlg;
        dlg.exec();
    });

    auto menuHelp = new Menu(tr("&Help"), q);
    menuHelp->addAction(actionCheckForUpdates);
    menuHelp->addAction(actionAbout);
    menuHelp->addAction(actionDiscoverDiffScope);
    return menuHelp;
}
