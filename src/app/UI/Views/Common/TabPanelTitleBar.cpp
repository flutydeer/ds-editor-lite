//
// Created by FlutyDeer on 2025/7/13.
//

#include "TabPanelTitleBar.h"

#include "Controller/AppController.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Controls/Button.h"
#include "UI/Controls/ToolTipFilter.h"
#include "Utils/SystemUtils.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QGraphicsOpacityEffect>
#include <QStackedWidget>
#include <QTabBar>
#include <QVariantAnimation>

static const auto ChromeMinimize = QStringLiteral(u"\ue921");
static const auto ChromeMaximize = QStringLiteral(u"\ue922");
static const auto ChromeRestore  = QStringLiteral(u"\ue923");
static const auto ChromeClose    = QStringLiteral(u"\ue8bb");

TabPanelTitleBar::TabPanelTitleBar(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);

    m_tabBar = new QTabBar;
    m_tabBar->setDrawBase(false);
    m_tabBar->setExpanding(false);
    m_tabBar->setMinimumWidth(12);

    m_toolBar = new QStackedWidget;

    m_btnLayout = new QHBoxLayout;
    m_btnLayout->setContentsMargins({});
    m_btnLayout->setSpacing(6);

    m_innerLayout = new QHBoxLayout;
    m_innerLayout->addWidget(m_tabBar);
    m_innerLayout->addWidget(m_toolBar);
    m_innerLayout->addStretch();
    m_innerLayout->addLayout(m_btnLayout);
    m_innerLayout->setSpacing(0);
    m_innerLayout->setContentsMargins(6, 6, 9, 6);

    m_systemBtnLayout = new QHBoxLayout;
    m_systemBtnLayout->setContentsMargins({});
    m_systemBtnLayout->setSpacing(0);

    m_outerLayout = new QHBoxLayout;
    m_outerLayout->addLayout(m_innerLayout, 1);
    m_outerLayout->addLayout(m_systemBtnLayout);
    m_outerLayout->setSpacing(0);
    m_outerLayout->setContentsMargins({});
    setLayout(m_outerLayout);

    buildDockedButtons();

    m_animation = new QVariantAnimation(this);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);

    setContentsMargins({});
}

QTabBar *const &TabPanelTitleBar::tabBar() const {
    return m_tabBar;
}

QStackedWidget *const &TabPanelTitleBar::toolBar() const {
    return m_toolBar;
}

Button *TabPanelTitleBar::minimizeButton() const {
    return m_btnMin;
}

Button *TabPanelTitleBar::maximizeButton() const {
    return m_btnMax;
}

Button *TabPanelTitleBar::closeButton() const {
    return m_btnClose;
}

void TabPanelTitleBar::setDetached(bool detached, bool useNativeFrame) {
    if (m_detached && window())
        window()->removeEventFilter(this);

    m_detached = detached;
    clearButtonLayout();
    if (detached) {
        buildDetachedButtons(useNativeFrame);

        if (!m_opacityEffect) {
            m_opacityEffect = new QGraphicsOpacityEffect(this);
            m_opacityEffect->setOpacity(1.0);
            setGraphicsEffect(m_opacityEffect);
            connect(m_animation, &QVariantAnimation::valueChanged, this,
                    [this](const QVariant &value) {
                        m_opacityEffect->setOpacity(value.toDouble());
                    });
        }
    } else {
        buildDockedButtons();

        if (m_opacityEffect) {
            m_animation->stop();
            setGraphicsEffect(nullptr);
            m_opacityEffect = nullptr;
        }
    }

    if (m_detached) {
        QMetaObject::invokeMethod(this, [this] {
            if (window())
                window()->installEventFilter(this);
        }, Qt::QueuedConnection);
    }
}

void TabPanelTitleBar::buildDockedButtons() {
    const int btnSize = 28;

    m_btnDetach = new Button;
    m_btnDetach->setObjectName("btnPanelDetach");
    m_btnDetach->setFixedSize(btnSize, btnSize);
    m_btnDetach->setToolTip(tr("Detach to window"));
    m_btnDetach->installEventFilter(new ToolTipFilter(m_btnDetach, 500, false, true));
    connect(m_btnDetach, &Button::clicked, this, &TabPanelTitleBar::detachRequested);

    m_btnMaximize = new Button;
    m_btnMaximize->setObjectName("btnPanelMaximize");
    m_btnMaximize->setFixedSize(btnSize, btnSize);
    m_btnMaximize->setCheckable(true);
    m_btnMaximize->setToolTip(tr("Maximize or restore"));
    m_btnMaximize->installEventFilter(new ToolTipFilter(m_btnMaximize, 500, false, true));
    connect(m_btnMaximize, &Button::clicked, this, [=] {
        if (appStatus->trackPanelCollapsed)
            appController->setTrackAndClipPanelCollapsed(false, false);
        else
            appController->setTrackAndClipPanelCollapsed(true, false);
    });
    connect(appStatus, &AppStatus::trackPanelCollapseStateChanged, m_btnMaximize,
            &Button::setChecked);

    m_btnHide = new Button;
    m_btnHide->setObjectName("btnPanelHide");
    m_btnHide->setFixedSize(btnSize, btnSize);
    m_btnHide->setToolTip(tr("Hide"));
    m_btnHide->installEventFilter(new ToolTipFilter(m_btnHide, 500, false, true));
    connect(m_btnHide, &Button::clicked, this,
            [=] { appController->setTrackAndClipPanelCollapsed(false, true); });

    m_btnLayout->addWidget(m_btnDetach);
    m_btnLayout->addWidget(m_btnMaximize);
    m_btnLayout->addWidget(m_btnHide);
}

void TabPanelTitleBar::buildDetachedButtons(bool useNativeFrame) {
    if (useNativeFrame)
        return;

    int systemButtonWidth = 48;

    m_btnMin = new Button;
    m_btnMin->setObjectName("MinimizeButton");
    m_btnMin->setFixedWidth(systemButtonWidth);

    m_btnMax = new Button;
    m_btnMax->setCheckable(true);
    m_btnMax->setObjectName("MaximizeButton");
    m_btnMax->setFixedWidth(systemButtonWidth);

    m_btnClose = new Button;
    m_btnClose->setObjectName("CloseButton");
    m_btnClose->setFixedWidth(systemButtonWidth);

    if (SystemUtils::isWindows()) {
        auto fontFamily =
            QSysInfo::productVersion() == "11" ? "Segoe Fluent Icons" : "Segoe MDL2 Assets";
        auto font = QFont(fontFamily);
        font.setPointSizeF(7.2);

        m_btnMin->setText(ChromeMinimize);
        m_btnMin->setFont(font);
        m_btnMax->setText(ChromeMaximize);
        m_btnMax->setFont(font);
        m_btnClose->setText(ChromeClose);
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

    m_systemBtnLayout->addWidget(m_btnMin);
    m_systemBtnLayout->addWidget(m_btnMax);
    m_systemBtnLayout->addWidget(m_btnClose);
}

void TabPanelTitleBar::clearButtonLayout() {
    auto safeDelete = [](Button *&btn) {
        if (btn) {
            btn->deleteLater();
            btn = nullptr;
        }
    };

    safeDelete(m_btnDetach);
    safeDelete(m_btnMaximize);
    safeDelete(m_btnHide);
    safeDelete(m_btnMin);
    safeDelete(m_btnMax);
    safeDelete(m_btnClose);

    m_btnLayout->setSpacing(6);
}

bool TabPanelTitleBar::eventFilter(QObject *watched, QEvent *event) {
    if (m_detached && watched == window()) {
        if (event->type() == QEvent::WindowActivate)
            setActiveStyle(true);
        else if (event->type() == QEvent::WindowDeactivate)
            setActiveStyle(false);
        else if (event->type() == QEvent::WindowStateChange && m_btnMax) {
            auto checked = window()->isMaximized();
            m_btnMax->setChecked(checked);
            if (SystemUtils::isWindows())
                m_btnMax->setText(checked ? ChromeRestore : ChromeMaximize);
            else
                m_btnMax->setIcon(checked
                                      ? QIcon(":svg/title-bar/restore_16_filled_white.svg")
                                      : QIcon(":svg/title-bar/maximize_16_filled_white.svg"));
        }
    }
    return QWidget::eventFilter(watched, event);
}

void TabPanelTitleBar::setActiveStyle(bool active) const {
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
}
