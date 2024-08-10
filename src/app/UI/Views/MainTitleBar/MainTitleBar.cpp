//
// Created by fluty on 24-7-30.
//

#define ChromeMinimize 0xE921
#define ChromeMaximize 0xE922
#define ChromeRestore  0xE923
#define ChromeClose    0xE8BB

#include "MainTitleBar.h"

#include "ActionButtonsView.h"
#include "MainMenuView.h"
#include "PlaybackView.h"
#include "Controller/AppController.h"
#include "Modules/History/HistoryManager.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/ToolTipFilter.h"

#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>

MainTitleBar::MainTitleBar(MainMenuView *menuView, QWidget *parent, bool useNativeFrame)
    : QWidget(parent), m_menuView(menuView), m_window(parent) {
    setAttribute(Qt::WA_StyledBackground);

    m_playbackView = new PlaybackView(this);

    auto menuBarContainer = new QHBoxLayout;
    menuBarContainer->addWidget(menuView);
    menuBarContainer->setContentsMargins({});

    m_actionButtonsView = new ActionButtonsView(this);
    connect(m_actionButtonsView, &ActionButtonsView::saveTriggered, menuView->actionSave(),
            &QAction::trigger);

    connect(historyManager, &HistoryManager::undoRedoChanged, appController,
            &AppController::onUndoRedoChanged);

    if (!useNativeFrame) {
        m_lbTitle = new QLabel;
        m_lbTitle->setMinimumWidth(8);

        auto fontFamily =
            QSysInfo::productVersion() == "11" ? "Segoe Fluent Icons" : "Segoe MDL2 Assets";
        auto font = QFont(fontFamily);
        font.setPointSizeF(7.2);
        // font.setHintingPreference(QFont::PreferDefaultHinting);
        int systemButtonWidth = 48;

        m_btnMin = new Button(QChar(ChromeMinimize));
        m_btnMin->setObjectName("MinimizeButton");
        // m_btnMin->setToolTip(tr("Minimize"));
        // m_btnMin->installEventFilter(new ToolTipFilter(m_btnMin));
        m_btnMin->setFont(font);
        m_btnMin->setFixedSize(systemButtonWidth, 40);

        m_btnMax = new Button(QChar(ChromeMaximize));
        m_btnMax->setObjectName("MaximizeButton");
        m_btnMax->setFont(font);
        m_btnMax->setCheckable(true);
        m_btnMax->setFixedSize(systemButtonWidth, 40);

        m_btnClose = new Button(QChar(ChromeClose));
        m_btnClose->setObjectName("CloseButton");
        m_btnClose->setFont(font);
        m_btnClose->setFixedSize(systemButtonWidth, 40);

        connect(m_btnMin, &Button::clicked, this, &MainTitleBar::minimizeTriggered);
        connect(m_btnMax, &Button::clicked, this, &MainTitleBar::maximizeTriggered);
        connect(m_btnClose, &Button::clicked, this, &MainTitleBar::closeTriggered);
    }

    auto mainLayout = new QHBoxLayout;
    if (!useNativeFrame) {
        // TODO: app icon
        mainLayout->addSpacerItem(new QSpacerItem(32, 20, QSizePolicy::Fixed));
    }
    mainLayout->addLayout(menuBarContainer);
    mainLayout->addWidget(m_actionButtonsView);
    mainLayout->addSpacerItem(new QSpacerItem(20, 20, QSizePolicy::Expanding));
    if (!useNativeFrame) {
        mainLayout->addWidget(m_lbTitle);
        mainLayout->addSpacerItem(new QSpacerItem(20, 20, QSizePolicy::Expanding));
    }
    mainLayout->addWidget(m_playbackView);
    if (!useNativeFrame) {
        mainLayout->addWidget(m_btnMin);
        mainLayout->addWidget(m_btnMax);
        mainLayout->addWidget(m_btnClose);
    }
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(6, 0, 0, 0);

    setLayout(mainLayout);
    setFixedHeight(40);
    m_opacityEffect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(m_opacityEffect);
}
MainMenuView *MainTitleBar::menuView() const {
    return m_menuView;
}
ActionButtonsView *MainTitleBar::actionButtonsView() const {
    return m_actionButtonsView;
}
PlaybackView *MainTitleBar::playbackView() const {
    return m_playbackView;
}
Button *MainTitleBar::minimizeButton() const {
    return m_btnMin;
}
Button *MainTitleBar::maximizeButton() const {
    return m_btnMax;
}
Button *MainTitleBar::closeButton() const {
    return m_btnClose;
}
void MainTitleBar::setTitle(const QString &title) const {
    if (!m_lbTitle)
        return;
    m_lbTitle->setText(title);
    // QFontMetrics fontWidth(m_lbTitle->font());
    // auto elidedText = fontWidth.elidedText(title, Qt::ElideRight, m_lbTitle->width());
    // m_lbTitle->setText(elidedText);
}
bool MainTitleBar::eventFilter(QObject *watched, QEvent *event) {
    if (watched != m_window)
        return QWidget::eventFilter(watched, event);

    if (event->type() == QEvent::WindowTitleChange) {
        setTitle(m_window->windowTitle());
    } else if (event->type() == QEvent::WindowStateChange) {
        auto checked = m_window->isMaximized();
        if (m_btnMax) {
            m_btnMax->setChecked(checked);
            m_btnMax->setText(checked ? QChar(ChromeRestore) : QChar(ChromeMaximize));
        }
    } else if (event->type() == QEvent::WindowActivate)
        setActiveStyle(true);
    else if (event->type() == QEvent::WindowDeactivate)
        setActiveStyle(false);
    // else if (event->type() == QEvent::Resize)
    //     setTitle(m_window->windowTitle());

    return QWidget::eventFilter(watched, event);
}
void MainTitleBar::setActiveStyle(bool active) {
    m_opacityEffect->setOpacity(active ? 1.0 : 0.5);
    // setProperty("windowActive", active);
    // style()->unpolish(this);
    // style()->polish(this);
}