//
// Created by fluty on 24-7-21.
//

#ifndef MAINMENUVIEW_P_H
#define MAINMENUVIEW_P_H

#include "Global/AppGlobal.h"

class QAction;
class MainWindow;

class MainMenuViewPrivate : QObject {
    Q_OBJECT

    Q_DECLARE_PUBLIC(MainMenuView)
public:
    explicit MainMenuViewPrivate(MainWindow *window) : m_mainWindow(window) {
    }

    MainWindow *m_mainWindow;

    QAction *m_actionUndo = nullptr;
    QAction *m_actionRedo = nullptr;
    QAction *m_actionSave = nullptr;
    QAction *m_actionSaveAs = nullptr;
    QAction *m_actionSelectAll = nullptr;
    QAction *m_actionDelete = nullptr;
    QAction *m_actionCut = nullptr;
    QAction *m_actionCopy = nullptr;
    QAction *m_actionPaste = nullptr;
    QAction *m_actionFillLyrics = nullptr;
    QAction *m_searchFillLyrics = nullptr;

    AppGlobal::PanelType m_panelType = AppGlobal::Generic;

    void onNewProject() const;
    void onOpenProject();
    // void onOpenAProject();
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
    void exitApp();

private:
    MainMenuView *q_ptr = nullptr;
};


#endif // MAINMENUVIEW_P_H
