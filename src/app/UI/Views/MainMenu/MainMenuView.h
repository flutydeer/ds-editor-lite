//
// Created by fluty on 2024/7/13.
//

#ifndef MAINMENUVIEW_H
#define MAINMENUVIEW_H

#include <QMenuBar>

#include "Global/AppGlobal.h"

class MainWindow;
class MainMenuView : public QMenuBar {
    Q_OBJECT

public:
    explicit MainMenuView(MainWindow *mainWindow);

    [[nodiscard]] QAction *actionSave() const;
    [[nodiscard]] QAction *actionSaveAs() const;

private slots:
    // File
    void onNewProject() const;
    void onOpenProject();
    void onOpenAProject();
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

private:
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

    AppGlobal::PanelType m_panelType;
};

#endif // MAINMENUVIEW_H
