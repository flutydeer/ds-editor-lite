//
// Created by fluty on 24-7-21.
//

#ifndef MAINMENUVIEW_P_H
#define MAINMENUVIEW_P_H

#include "Global/AppGlobal.h"

#include <QColor>
#include <QHash>
#include <QObject>
#include <QPair>
#include <QSize>

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

    // Theme colors for tinted menu icons (exposed as QSS properties on MainMenuView)
    QColor m_iconColor = {0xE0, 0xE0, 0xE0};
    QColor m_iconDisabledColor = {0x90, 0x90, 0x90};
    // Actions with tinted SVG icons and their source paths/sizes, for re-tinting
    QHash<QAction *, QPair<QString, QSize>> m_tintedActions;

    // Tint the SVG at path with the current theme colors, apply it to the
    // action, and register the action for later re-tinting
    void setMenuIcon(QAction *action, const QString &path, const QSize &size = QSize(16, 16));
    // Re-tint all registered action icons from the current theme colors
    void rebuildIcons();

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

    QAction *actionGeneralOptions = nullptr;
    QAction *actionAudioSettings = nullptr;
    QAction *actionMidiSettings = nullptr;
    QAction *actionAppearanceOptions = nullptr;
    QAction *actionInferenceOptions = nullptr;
    QAction *actionDeveloperOptions = nullptr;
    QAction *actionCheckForUpdates = nullptr;
    QAction *actionAbout = nullptr;
    QAction *actionDiscoverDiffScope = nullptr;

    Menu *menuFile = nullptr;
    Menu *menuRecentProjects = nullptr;
    Menu *menuImport = nullptr;
    Menu *menuExport = nullptr;
    Menu *menuEdit = nullptr;
    Menu *menuOptions = nullptr;
    Menu *menuHelp = nullptr;

    QString m_undoName;
    QString m_redoName;

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
    void retranslateUi();

private:
    MainMenuView *q_ptr = nullptr;
};


#endif // MAINMENUVIEW_P_H
