//
// Created by fluty on 2024/7/13.
//

#ifndef MAINMENUVIEW_H
#define MAINMENUVIEW_H

#include <QMenuBar>

class MainMenuView : public QMenuBar {
    Q_OBJECT

public:
    explicit MainMenuView(QWidget *parent = nullptr);

    [[nodiscard]] QAction *actionSave() const;

private:
    QAction *m_actionSave = nullptr;
};

#endif // MAINMENUVIEW_H
