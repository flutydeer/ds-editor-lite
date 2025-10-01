//
// Created by fluty on 2024/7/13.
//

#ifndef MAINMENUVIEW_H
#define MAINMENUVIEW_H

#include <QMenuBar>

class MainMenuViewPrivate;
class MainWindow;

class MainMenuView : public QMenuBar {
    Q_OBJECT

public:
    explicit MainMenuView(MainWindow *mainWindow);
    ~MainMenuView() override;

    [[nodiscard]] QAction *actionSave();
    [[nodiscard]] QAction *actionSaveAs();

private:
    Q_DECLARE_PRIVATE(MainMenuView)
    QScopedPointer<MainMenuViewPrivate> d_ptr;
};

#endif // MAINMENUVIEW_H
