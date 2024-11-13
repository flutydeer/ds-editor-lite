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

    QAction *actionUndo = nullptr;
    QAction *actionRedo = nullptr;
    QAction *actionSave = nullptr;
    QAction *actionSaveAs = nullptr;
    QAction *actionSelectAll = nullptr;
    QAction *actionDelete = nullptr;
    QAction *actionCut = nullptr;
    QAction *actionCopy = nullptr;
    QAction *actionPaste = nullptr;
    QAction *actionFillLyrics = nullptr;
    QAction *actionSearchLyrics = nullptr;
    QAction *actionGetPitchParamFromAudioClip = nullptr;

    AppGlobal::PanelType m_panelType = AppGlobal::Generic;

    void onNew() const;
    void onOpen();
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
    void onGetPitchParamFromAudioClip();
    void exitApp();

private:
    MainMenuView *q_ptr = nullptr;
};


#endif // MAINMENUVIEW_P_H
