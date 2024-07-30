//
// Created by fluty on 24-7-30.
//

#include "MainTitleBar.h"

#include "Controller/AppController.h"
#include "Modules/History/HistoryManager.h"
#include "UI/Controls/Button.h"
#include "UI/Views/ActionButtonsView.h"
#include "UI/Views/PlaybackView.h"
#include "UI/Views/MainMenu/MainMenuView.h"

#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>

MainTitleBar::MainTitleBar(MainMenuView *menuView, QWidget *parent)
    : QWidget(parent), m_menuView(menuView), m_window(parent) {
    setAttribute(Qt::WA_StyledBackground);

    m_playbackView = new PlaybackView(this);

    auto menuBarContainer = new QHBoxLayout;
    menuBarContainer->addWidget(menuView);
    menuBarContainer->setContentsMargins(0, 6, 0, 6);

    m_actionButtonsView = new ActionButtonsView(this);
    connect(m_actionButtonsView, &ActionButtonsView::saveTriggered, menuView->actionSave(),
            &QAction::trigger);

    connect(historyManager, &HistoryManager::undoRedoChanged, appController,
            &AppController::onUndoRedoChanged);

    m_lbTitle = new QLabel(this);

    m_btnMin = new Button("Min", this);
    m_btnMin->setObjectName("MinimizeButton");
    m_btnMin->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    m_btnMax = new Button("Max", this);
    m_btnMax->setObjectName("MaximizeButton");
    m_btnMax->setCheckable(true);
    m_btnClose = new Button("Close", this);
    m_btnClose->setObjectName("CloseButton");
    connect(m_btnMin, &Button::clicked, this, &MainTitleBar::minimizeTriggered);
    connect(m_btnMax, &Button::clicked, this, &MainTitleBar::maximizeTriggered);
    connect(m_btnClose, &Button::clicked, this, &MainTitleBar::closeTriggered);

    auto mainLayout = new QHBoxLayout;
    mainLayout->addLayout(menuBarContainer);
    mainLayout->addWidget(m_actionButtonsView);
    mainLayout->addSpacerItem(new QSpacerItem(20, 20, QSizePolicy::Expanding));
    mainLayout->addWidget(m_lbTitle);
    mainLayout->addSpacerItem(new QSpacerItem(20, 20, QSizePolicy::Expanding));
    mainLayout->addWidget(m_playbackView);
    mainLayout->addWidget(m_btnMin);
    mainLayout->addWidget(m_btnMax);
    mainLayout->addWidget(m_btnClose);
    mainLayout->setContentsMargins({});

    setLayout(mainLayout);
    m_opacityEffect = new QGraphicsOpacityEffect;
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
    m_lbTitle->setText(title);
}
bool MainTitleBar::eventFilter(QObject *watched, QEvent *event) {
    if (watched != m_window)
        return QWidget::eventFilter(watched, event);

    if (event->type() == QEvent::WindowTitleChange)
        m_lbTitle->setText(m_window->windowTitle());
    else if (event->type() == QEvent::WindowStateChange)
        m_btnMax->setChecked(m_window->isMaximized());
    else if (event->type() == QEvent::WindowActivate)
        setActiveStyle(true);
    else if (event->type() == QEvent::WindowDeactivate)
        setActiveStyle(false);

    return QWidget::eventFilter(watched, event);
}
void MainTitleBar::setActiveStyle(bool active) {
    m_opacityEffect->setOpacity(active ? 1.0 : 0.5);
    // setProperty("windowActive", active);
    // style()->unpolish(this);
    // style()->polish(this);
}