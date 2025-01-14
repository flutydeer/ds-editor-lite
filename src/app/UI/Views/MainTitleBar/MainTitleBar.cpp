//
// Created by fluty on 24-7-30.
//

#include "MainTitleBar.h"

#include "ActionButtonsView.h"
#include "MainMenuView.h"
#include "PlaybackView.h"
#include "Controller/AppController.h"
#include "Modules/History/HistoryManager.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/ToolTipFilter.h"
#include "Utils/SystemUtils.h"

#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QVariantAnimation>

enum {
    ChromeMinimize = 0xE921,
    ChromeMaximize = 0xE922,
    ChromeRestore = 0xE923,
    ChromeClose = 0xE8BB
};

MainTitleBar::MainTitleBar(MainMenuView *menuView, QWidget *parent, bool useNativeFrame)
    : QWidget(parent), m_window(parent), m_menuView(menuView) {
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

        int systemButtonWidth = 48;

        m_btnMin = new Button;
        m_btnMin->setObjectName("MinimizeButton");
        m_btnMin->setFixedSize(systemButtonWidth, 40);

        m_btnMax = new Button;
        m_btnMax->setCheckable(true);
        m_btnMax->setObjectName("MaximizeButton");
        m_btnMax->setFixedSize(systemButtonWidth, 40);

        m_btnClose = new Button;
        m_btnClose->setObjectName("CloseButton");
        m_btnClose->setFixedSize(systemButtonWidth, 40);

        if (SystemUtils::isWindows()) {
            auto fontFamily =
                QSysInfo::productVersion() == "11" ? "Segoe Fluent Icons" : "Segoe MDL2 Assets";
            auto font = QFont(fontFamily);
            font.setPointSizeF(7.2);

            m_btnMin->setText(QChar(ChromeMinimize));
            m_btnMin->setFont(font);
            m_btnMax->setText(QChar(ChromeMaximize));
            m_btnMax->setFont(font);
            m_btnClose->setText(QChar(ChromeClose));
            m_btnClose->setFont(font);
        } else {
            constexpr auto icoSize = QSize(14, 14);
            m_btnMin->setIconSize(icoSize);
            m_btnMin->setIcon(QIcon(":svg/title-bar/minimize_16_filled_white.svg"));
            m_btnMax->setIconSize(icoSize);
            m_btnMax->setIcon(QIcon(":svg/title-bar/maximize_16_filled_white.svg"));
            m_btnClose->setIconSize(icoSize);
            m_btnClose->setIcon(QIcon(":svg/title-bar/close_16_filled_white.svg"));
        }


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
    mainLayout->addSpacing(6);
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
    m_animation = new QVariantAnimation(this);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged, this,
            [=](const QVariant &value) { m_opacityEffect->setOpacity(value.toDouble()); });
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
            if (SystemUtils::isWindows())
                m_btnMax->setText(checked ? QChar(ChromeRestore) : QChar(ChromeMaximize));
            else
                m_btnMax->setIcon(checked ? QIcon(":svg/title-bar/restore_16_filled_white.svg")
                                          : QIcon(":svg/title-bar/maximize_16_filled_white.svg"));
        }
    } else if (event->type() == QEvent::WindowActivate)
        setActiveStyle(true);
    else if (event->type() == QEvent::WindowDeactivate)
        setActiveStyle(false);
    // else if (event->type() == QEvent::Resize)
    //     setTitle(m_window->windowTitle());

    return QWidget::eventFilter(watched, event);
}

void MainTitleBar::setActiveStyle(bool active) const {
    m_animation->stop();
    m_animation->setStartValue(m_opacityEffect->opacity());
    if (active) {
        m_animation->setEndValue(1.0);
        m_animation->setDuration(100);
    } else {
        m_animation->setEndValue(0.5);
        m_animation->setDuration(300);
    }
    m_animation->start();
    // m_opacityEffect->setOpacity(active ? 1.0 : 0.5);
    // setProperty("windowActive", active);
    // style()->unpolish(this);
    // style()->polish(this);
}