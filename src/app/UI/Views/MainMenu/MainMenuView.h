//
// Created by fluty on 2024/7/13.
//

#ifndef MAINMENUVIEW_H
#define MAINMENUVIEW_H

#include <QMenuBar>

#include "Global/AppGlobal.h"

class MainMenuView : public QMenuBar {
    Q_OBJECT

public:
    explicit MainMenuView(QWidget *parent = nullptr);

    [[nodiscard]] QAction *actionSave() const;

private slots:
    void onActivatedPanelChanged(AppGlobal::PanelType panel);
    void onSelectAll();
    void onDelete();
    void onCut();
    void onCopy();
    void onPaste();

private:
    QAction *m_actionSave = nullptr;
    QAction *m_actionSelectAll = nullptr;
    QAction *m_actionDelete = nullptr;
    QAction *m_actionCut = nullptr;
    QAction *m_actionCopy = nullptr;
    QAction *m_actionPaste = nullptr;

    AppGlobal::PanelType m_panelType;
};

#endif // MAINMENUVIEW_H
