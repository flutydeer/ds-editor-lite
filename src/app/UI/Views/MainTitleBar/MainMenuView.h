#ifndef MAINMENUVIEW_H
#define MAINMENUVIEW_H

#include "TitleBarSections.h"

#include <QMenuBar>
#include <QList>

class QAction;
class MainMenuViewPrivate;
class MainWindow;
class QString;
class Menu;

class MainMenuView : public QMenuBar {
    Q_OBJECT
    Q_PROPERTY(QColor iconColor READ iconColor WRITE setIconColor)
    Q_PROPERTY(QColor iconDisabledColor READ iconDisabledColor WRITE setIconDisabledColor)

public:
    explicit MainMenuView(MainWindow *mainWindow,
                          TitleBarRole role = TitleBarRole::Primary);
    ~MainMenuView() override;

    [[nodiscard]] QAction *actionNew();
    [[nodiscard]] QAction *actionOpen();
    [[nodiscard]] QAction *actionSave();
    [[nodiscard]] QAction *actionSaveAs();
    void openRecentProject(const QString &filePath);

    // A normal QMenuBar action owns the ellipsis button. It participates in the
    // ordinary action layout, so it always sits right after the last visible
    // menu instead of being pinned to the far right by QMenuBar's private
    // extension widget. Collapsed entries are the original menu actions.
    [[nodiscard]] QAction *overflowAction() const;
    [[nodiscard]] Menu *overflowMenu() const;
    [[nodiscard]] QList<QAction *> topLevelMenuActions() const;
    [[nodiscard]] QList<QAction *> overflowedMenuActions() const;
    [[nodiscard]] bool isOverflowing() const;

    // Whether this menu bar renders its actions inside the window. False when
    // the platform owns the menu bar (macOS global menu bar, Unity's dbusmenu)
    // or when the title bar hides it. The adaptive overflow split only applies
    // to an in-window menu bar: collapsing detaches the menu actions, and the
    // platform menu bar drops them on ActionRemoved.
    [[nodiscard]] bool isInWindowMenu() const;
    void setInWindowMenu(bool inWindow);

    // Both hints are derived from the full menu set, never from the current
    // split, so resizing cannot feed back into the enclosing title bar layout.
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    bool event(QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    Q_DECLARE_PRIVATE(MainMenuView)
    QScopedPointer<MainMenuViewPrivate> d_ptr;

    [[nodiscard]] int menuItemWidth(const QAction *action) const;

    // Theme color accessors (QSS-overridable via qproperty-*); setters
    // re-tint the already-generated menu icons
    [[nodiscard]] QColor iconColor() const;
    void setIconColor(const QColor &color);
    [[nodiscard]] QColor iconDisabledColor() const;
    void setIconDisabledColor(const QColor &color);
};

#endif // MAINMENUVIEW_H
