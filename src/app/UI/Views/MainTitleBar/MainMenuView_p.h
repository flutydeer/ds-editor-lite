#ifndef MAINMENUVIEW_P_H
#define MAINMENUVIEW_P_H

#include "Global/AppGlobal.h"
#include "Interface/EditorInteraction.h"
#include "UI/Views/MainTitleBar/TitleBarSections.h"

#include <QColor>
#include <QHash>
#include <QKeySequence>
#include <QList>
#include <QObject>
#include <QPair>
#include <QPointer>
#include <QSize>

class QAction;
class Menu;
class MainMenuView;
class MainWindow;

class MainMenuViewPrivate : QObject {
    Q_OBJECT

    Q_DECLARE_PUBLIC(MainMenuView)
public:
    explicit MainMenuViewPrivate(MainWindow *window, const TitleBarRole role)
        : m_mainWindow(window), m_role(role) {
    }

    MainWindow *m_mainWindow;
    TitleBarRole m_role = TitleBarRole::Primary;

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
    QAction *actionImportDspx = nullptr;
    QAction *actionImportLibreSVIP = nullptr;
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
    QAction *actionQuantize = nullptr;
    QAction *actionFillLyrics = nullptr;
    QAction *actionSearchLyrics = nullptr;
    QAction *actionExtractPitchParam = nullptr;

    QAction *actionGeneralOptions = nullptr;
    QAction *actionAudioSettings = nullptr;
    QAction *actionMidiSettings = nullptr;
    QAction *actionAppearanceOptions = nullptr;
    QAction *actionInferenceOptions = nullptr;
    QAction *actionAutomationOptions = nullptr;
    QAction *actionDeveloperOptions = nullptr;
    QAction *actionCheckForUpdates = nullptr;
    QAction *actionAbout = nullptr;
    QAction *actionOpenLogFolder = nullptr;
    QAction *actionOpenDumpFolder = nullptr;

    Menu *menuFile = nullptr;
    Menu *menuRecentProjects = nullptr;
    Menu *menuImport = nullptr;
    Menu *menuExport = nullptr;
    Menu *menuEdit = nullptr;
    Menu *menuOptions = nullptr;
    Menu *menuHelp = nullptr;

    // All geometry decisions are made from the full menu set. Qt's private
    // extension button is never used or styled.
    QPointer<QAction> m_overflowAction;
    QPointer<Menu> m_overflowMenu;
    QList<QPointer<QAction>> m_topLevelMenus;
    bool m_updatingOverflow = false;
    // The menu bar renders its actions inside the window; see
    // MainMenuView::setInWindowMenu.
    bool m_inWindowMenu = true;
    // Number of top-level menus currently inline on the menu bar. -1 forces the
    // first update to rebuild both action lists.
    int m_keepCount = -1;

    void initOverflowUi();
    void refreshOverflowIcon();
    void retranslateOverflowUi();
    void updateOverflow();
    void syncOverflowActions(int keepCount);
    [[nodiscard]] int inlineRightEdge(const QList<QAction *> &items, int start,
                                      int spacing) const;
    [[nodiscard]] int fullInlineWidth() const;
    [[nodiscard]] int minimumInlineWidth() const;

    QString m_undoName;
    QString m_redoName;

    EditorInteraction::Target m_editTarget = EditorInteraction::Target::None;
    QMetaObject::Connection m_clipCanSelectAllConnection;
    QMetaObject::Connection m_noteSelectionConnection;
    QMetaObject::Connection m_clipSelectionConnection;

    void onNew() const;
    void onOpen();
    void onOpenRecentProject(const QString &filePath);
    void onClearRecentProjects();
    void refreshRecentProjectsMenu();
    void openFileWithSavePrompt(const QString &filePath);
    void onImportMidiFile();
    void onImportDspxFile();
    void onImportLibreSVIPFile();
    void onExportMidiFile();
    void onExportAudioFile();
    void onUndoRedoChanged(bool canUndo, const QString &undoName, bool canRedo,
                           const QString &redoName);

    void onActiveEditTargetChanged(EditorInteraction::Target target);
    void onEditCommandCapabilitiesChanged();
    void onSelectAll();
    void onDelete();
    void onCut();
    void onCopy();
    void onPaste();
    void onExtractPitchParam();
    void onOctaveUp();
    void onOctaveDown();
    void onQuantize();
    void exitApp();

    void enterClipEditorState();
    void exitClipEditorState();
    void enterTracksEditorState();
    void exitTracksEditorState();
    void enterParametersEditorState();
    void exitParametersEditorState();
    void updateClipEditorActionState();
    void updatePasteActionState();

    void initActions();
    void initFileActions();
    void initEditActions();
    void initEditShortcuts();
    // Registers a key sequence only on the primary window's menu. Secondary
    // menus show the same actions without claiming a second global shortcut.
    void setActionShortcut(QAction *action, const QKeySequence &key,
                           Qt::ShortcutContext context);

    Menu *buildFileMenu();
    Menu *buildEditMenu();
    Menu *buildOptionsMenu();
    Menu *buildHelpMenu();
    void retranslateUi();

private:
    MainMenuView *q_ptr = nullptr;
};


#endif // MAINMENUVIEW_P_H
