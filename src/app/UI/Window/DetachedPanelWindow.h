#ifndef DETACHEDPANELWINDOW_H
#define DETACHEDPANELWINDOW_H

#include "Base/Window.h"

#include <QPointer>
#include <QString>

class QHBoxLayout;
class QVBoxLayout;
class MainWindow;
class MainMenuView;
class MainTitleBar;

namespace QWK {
    class WidgetWindowAgent;
}

// The detached editor panel is a true top-level window: it has no QWidget
// parent. A parented top-level is still a secondary window in Qt, and Windows
// turns its transient parent into a native owner, which means no taskbar button,
// an unconditional z-order above the owner, and a minimize that belongs to the
// owner. MainWindow owns this window explicitly instead, so it can live on its
// own while still being destroyed with the primary window.
// Window is the shared base that registers a top-level widget with ThemeManager
// for frame effects. Deriving from it is what makes the native border follow the
// light/dark scheme exactly like the main window does.
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

    // The window title is the document title plus this suffix, so the taskbar
    // entry stays distinguishable from the main window. Empty means none.
    void setTitleSuffix(const QString &suffix);

signals:
    void reattachRequested();

protected:
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void buildChrome(MainWindow *mainWindow, bool useNativeFrame);
    void setupWindowAgent();
    void refreshWindowTitle();

    QPointer<MainWindow> m_mainWindow;
    MainMenuView *m_menuView = nullptr;
    MainTitleBar *m_titleBar = nullptr;
    QWidget *m_contentHost = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
    QWidget *m_content = nullptr;
    QWK::WidgetWindowAgent *m_agent = nullptr;
    QString m_titleSuffix;
    bool m_useNativeFrame = false;
};

#endif // DETACHEDPANELWINDOW_H
