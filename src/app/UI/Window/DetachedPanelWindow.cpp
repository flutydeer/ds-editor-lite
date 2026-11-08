#include "DetachedPanelWindow.h"

#include "MainWindow.h"
#include "UI/Views/Common/EditorShortcutUtils.h"
#include "UI/Views/MainTitleBar/MainMenuView.h"
#include "UI/Views/MainTitleBar/MainTitleBar.h"
#include "UI/Views/MainTitleBar/PlaybackView.h"
#include "UI/Views/MainTitleBar/TitleBarSections.h"
#include <lite/GUI/Controls/SystemWindowButton.h>
#include <lite/GUI/Theme/ThemeManager.h>

#include <QCloseEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWKWidgets/widgetwindowagent.h>

namespace {
    // A detached panel is an editing surface, not a compact tool palette.
    constexpr int kMinimumWindowWidth = 960;
    // Mirrors the main window: the panel card is inset from the window edges.
    constexpr int kFrameSideMargin = 6;
    constexpr int kFrameBottomMargin = 6;
} // namespace

DetachedPanelWindow::DetachedPanelWindow(MainWindow *mainWindow, const bool useNativeFrame)
    : Window(mainWindow, Qt::Window), m_mainWindow(mainWindow),
      m_useNativeFrame(useNativeFrame) {
    setAttribute(Qt::WA_StyledBackground);
    setProperty(EditorShortcutUtils::kEditorWindowProperty, true);
    setWindowTitle(mainWindow ? mainWindow->windowTitle() : QString());
    if (mainWindow) {
        connect(mainWindow, &QWidget::windowTitleChanged, this, &QWidget::setWindowTitle);
    }

    buildChrome(mainWindow, useNativeFrame);
    if (!useNativeFrame)
        setupWindowAgent();

    setMinimumWidth(kMinimumWindowWidth);
    ThemeManager::instance()->addStyleRoot(this);
}

DetachedPanelWindow::~DetachedPanelWindow() {
    ThemeManager::instance()->removeStyleRoot(this);
    // The content belongs to the caller. Never let the child window delete it.
    if (m_content) {
        m_contentLayout->removeWidget(m_content);
        m_content->setParent(nullptr);
    }
    delete m_agent;
}

void DetachedPanelWindow::buildChrome(MainWindow *mainWindow, const bool useNativeFrame) {
    m_menuView = new MainMenuView(mainWindow, TitleBarRole::Secondary);
    m_titleBar = new MainTitleBar(m_menuView, this,
                                  TitleBarSections::forDetachedPanel(useNativeFrame),
                                  TitleBarRole::Secondary);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins({});
    rootLayout->setSpacing(0);
    rootLayout->addWidget(m_titleBar);

    m_contentHost = new QWidget;
    m_contentLayout = new QVBoxLayout(m_contentHost);
    m_contentLayout->setContentsMargins(kFrameSideMargin, 0, kFrameSideMargin,
                                        kFrameBottomMargin);
    m_contentLayout->setSpacing(0);
    rootLayout->addWidget(m_contentHost, 1);
}

void DetachedPanelWindow::setupWindowAgent() {
    m_agent = new QWK::WidgetWindowAgent(this);
    m_agent->setup(this);
    m_agent->setTitleBar(m_titleBar);
    m_agent->setSystemButton(QWK::WindowAgentBase::Minimize, m_titleBar->minimizeButton());
    m_agent->setSystemButton(QWK::WindowAgentBase::Maximize, m_titleBar->maximizeButton());
    m_agent->setSystemButton(QWK::WindowAgentBase::Close, m_titleBar->closeButton());
    m_agent->setHitTestVisible(m_menuView);
    if (auto *playback = m_titleBar->playbackView())
        m_agent->setHitTestVisible(playback);

    connect(m_titleBar, &MainTitleBar::minimizeTriggered, this, &QWidget::showMinimized);
    connect(m_titleBar, &MainTitleBar::maximizeTriggered, this, [this](const bool max) {
        if (max)
            showMaximized();
        else
            showNormal();
    });
    connect(m_titleBar, &MainTitleBar::closeTriggered, this,
            &DetachedPanelWindow::reattachRequested);
}

void DetachedPanelWindow::setContent(QWidget *content) {
    if (!content || m_content == content)
        return;
    content->setParent(m_contentHost);
    m_contentLayout->addWidget(content);
    m_content = content;
    content->show();
}

QWidget *DetachedPanelWindow::takeContent() {
    if (!m_content)
        return nullptr;
    auto *content = m_content;
    m_contentLayout->removeWidget(content);
    content->setParent(nullptr);
    m_content = nullptr;
    return content;
}

QWidget *DetachedPanelWindow::contentWidget() const {
    return m_content;
}

MainTitleBar *DetachedPanelWindow::titleBar() const {
    return m_titleBar;
}

MainMenuView *DetachedPanelWindow::menuView() const {
    return m_menuView;
}

void DetachedPanelWindow::closeEvent(QCloseEvent *event) {
    // The panel is a singleton child of the main window. Closing this window
    // means docking it back, not destroying the editing state.
    event->ignore();
    emit reattachRequested();
}
