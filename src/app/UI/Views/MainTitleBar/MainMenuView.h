//
// Created by fluty on 2024/7/13.
//

#ifndef MAINMENUVIEW_H
#define MAINMENUVIEW_H

#include <QMenuBar>

class MainMenuViewPrivate;
class MainWindow;
class QString;

class MainMenuView : public QMenuBar {
    Q_OBJECT
    Q_PROPERTY(QColor iconColor READ iconColor WRITE setIconColor)
    Q_PROPERTY(QColor iconDisabledColor READ iconDisabledColor WRITE setIconDisabledColor)

public:
    explicit MainMenuView(MainWindow *mainWindow);
    ~MainMenuView() override;

    [[nodiscard]] QAction *actionNew();
    [[nodiscard]] QAction *actionOpen();
    [[nodiscard]] QAction *actionSave();
    [[nodiscard]] QAction *actionSaveAs();
    void openRecentProject(const QString &filePath);

protected:
    void changeEvent(QEvent *event) override;

private:
    Q_DECLARE_PRIVATE(MainMenuView)
    QScopedPointer<MainMenuViewPrivate> d_ptr;

    // Theme color accessors (QSS-overridable via qproperty-*); setters
    // re-tint the already-generated menu icons
    [[nodiscard]] QColor iconColor() const;
    void setIconColor(const QColor &color);
    [[nodiscard]] QColor iconDisabledColor() const;
    void setIconDisabledColor(const QColor &color);
};

#endif // MAINMENUVIEW_H
