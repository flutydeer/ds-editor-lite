//
// Created by fluty on 24-7-21.
//

#ifndef MAINMENUVIEW_P_H
#define MAINMENUVIEW_P_H

#include "Global/AppGlobal.h"

#include <QObject>

class QAction;
class Menu;
class MainMenuView;
class MainWindow;

class MainMenuViewPrivate : QObject {
    Q_OBJECT

    Q_DECLARE_PUBLIC(MainMenuView)
public:
    explicit MainMenuViewPrivate(MainWindow *window) : m_mainWindow(window) {
    }

    MainWindow *m_mainWindow;

    QAction *actionNew = nullptr;
    QAction *actionOpen = nullptr;
    QAction *actionClearRecentProjects = nullptr;
    QAction *actionImportMidi = nullptr;
    QAction *actionExportAudio = nullptr;
    QAction *actionExportMidi = nullptr;
    QAction *actionOpenPackageManager = nullptr;
    QAction *actionExit = nullptr;

    QAction *actionUndo = nullptr;
    QAction *actionRedo = nullptr;
    QAction *actionSave = nullptr;
    QAction *actionSaveAs = nullptr;
    QAction *actionSelectAll = nullptr;
    QAction *actionDelete = nullptr;
    QAction *actionCut = nullptr;
    QAction *actionCopy = nullptr;
    QAction *actionPaste = nullptr;
    QAction *actionOctaveUp = nullptr;
    QAction *actionOctaveDown = nullptr;
    QAction *actionFillLyrics = nullptr;
    QAction *actionSearchLyrics = nullptr;
    QAction *actionExtractPitchParam = nullptr;

    Menu *menuRecentProjects = nullptr;

    AppGlobal::PanelType m_panelType = AppGlobal::Generic;

    void onNew() const;
    void onOpen();
    void onOpenRecentProject(const QString &filePath);
    void onClearRecentProjects();
    void refreshRecentProjectsMenu();
    void openFileWithSavePrompt(const QString &filePath);
    void onImportMidiFile();
    void onExportMidiFile();
    void onExportAudioFile();
    void onUndoRedoChanged(bool canUndo, const QString &undoName, bool canRedo,
                           const QString &redoName);

    void onActivatedPanelChanged(AppGlobal::PanelType panel);
    void onSelectAll();
    void onDelete();
    void onCut();
    void onCopy();
    void onPaste();
    void onExtractPitchParam();
    void onOctaveUp();
    void onOctaveDown();
    void exitApp();

    void enterClipEditorState();
    void exitClipEditorState();
    void enterTracksEditorState();
    void exitTracksEditorState();
    void updatePasteActionState();

    void initActions();
    void initFileActions();
    void initEditActions();

    Menu *buildFileMenu();
    Menu *buildEditMenu();
    Menu *buildOptionsMenu();
    Menu *buildHelpMenu();

private:
    MainMenuView *q_ptr = nullptr;
};


#endif // MAINMENUVIEW_P_H
