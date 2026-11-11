#ifndef DETACHEDPANELWINDOW_H
#define DETACHEDPANELWINDOW_H

#include "Base/Window.h"

class QHBoxLayout;
class QVBoxLayout;
class MainWindow;
class MainMenuView;
class MainTitleBar;

namespace QWK {
    class WidgetWindowAgent;
}

// The detached editor panel is a top-level window in its own right. It owns the
// chrome shared with the main window and the frame that keeps the panel card
// inset from the window edges. The panel itself stays a plain child widget, so
// docking it back is just a reparent, and all window-level concerns (QWindowKit
// agent, title, activation, theme registration, system buttons) live here
// instead of being spread across MainWindow.
// Window is the shared base that registers a top-level widget with
// ThemeManager for frame effects. Deriving from it is what makes the native
// border follow the light/dark scheme exactly like the main window does.
class DetachedPanelWindow : public Window {
    Q_OBJECT

public:
    explicit DetachedPanelWindow(MainWindow *mainWindow, bool useNativeFrame);
    ~DetachedPanelWindow() override;

    // Transfers ownership of the widget into the inset content area. The caller
    // keeps ownership of the object itself and must call takeContent() before
    // destroying this window.
    void setContent(QWidget *content);
    [[nodiscard]] QWidget *takeContent();
    [[nodiscard]] QWidget *contentWidget() const;
    [[nodiscard]] MainTitleBar *titleBar() const;
    [[nodiscard]] MainMenuView *menuView() const;

signals:
    void reattachRequested();

protected:
    bool event(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void buildChrome(MainWindow *mainWindow, bool useNativeFrame);
    void setupWindowAgent();

    MainWindow *m_mainWindow = nullptr;
    MainMenuView *m_menuView = nullptr;
    MainTitleBar *m_titleBar = nullptr;
    QWidget *m_contentHost = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
    QWidget *m_content = nullptr;
    QWK::WidgetWindowAgent *m_agent = nullptr;
    bool m_useNativeFrame = false;
};

#endif // DETACHEDPANELWINDOW_H
