#include "TabPanelTitleBar.h"

#include "Controller/EditorViewController.h"
#include "Model/AppStatus/AppStatus.h"
#include "SegmentedTabBar.h"
#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/ToolTipFilter.h>
#include <lite/GUI/Utils/IconUtils.h>

#include <QEvent>
#include <QHBoxLayout>
#include <QGraphicsOpacityEffect>
#include <QStackedWidget>
#include <QVariantAnimation>

TabPanelTitleBar::TabPanelTitleBar(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);

    m_tabBar = new SegmentedTabBar;

    m_toolBar = new QStackedWidget;

    m_btnLayout = new QHBoxLayout;
    m_btnLayout->setContentsMargins({});
    m_btnLayout->setSpacing(6);

    auto *layout = new QHBoxLayout;
    layout->addWidget(m_tabBar);
    layout->addSpacing(24);
    layout->addWidget(m_toolBar);
    layout->addStretch();
    layout->addLayout(m_btnLayout);
    layout->setSpacing(0);
    layout->setContentsMargins(5, 6, 8, 6);
    setLayout(layout);

    buildDockedButtons();
    updateButtonVisibility();

    m_animation = new QVariantAnimation(this);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
                if (m_opacityEffect)
                    m_opacityEffect->setOpacity(value.toDouble());
            });

    setContentsMargins({});
    initializeAnimation();
}

SegmentedTabBar *const &TabPanelTitleBar::tabBar() const {
    return m_tabBar;
}

QStackedWidget *const &TabPanelTitleBar::toolBar() const {
    return m_toolBar;
}

void TabPanelTitleBar::setDetachButtonVisible(const bool visible) {
    m_detachEnabled = visible;
    updateButtonVisibility();
}

void TabPanelTitleBar::updateButtonVisibility() {
    // A detached panel has no dock controls: the window chrome owns window
    // management and the detach action would be a no-op.
    if (m_btnDetach)
        m_btnDetach->setVisible(!m_detached && m_detachEnabled);
    if (m_btnMaximize)
        m_btnMaximize->setVisible(!m_detached);
    if (m_btnHide)
        m_btnHide->setVisible(!m_detached);
}

void TabPanelTitleBar::setDetached(const bool detached) {
    if (m_detached == detached)
        return;
    if (m_detached && window())
        window()->removeEventFilter(this);

    m_detached = detached;
    updateButtonVisibility();

    if (detached) {
        // QWidget::setGraphicsEffect() takes ownership and deletes the old
        // effect, so each detached session gets a fresh one.
        m_opacityEffect = new QGraphicsOpacityEffect(this);
        m_opacityEffect->setOpacity(1.0);
        setGraphicsEffect(m_opacityEffect);
    } else {
        m_animation->stop();
        setGraphicsEffect(nullptr);
        m_opacityEffect = nullptr;
    }

    if (m_detached) {
        QMetaObject::invokeMethod(
            this,
            [this] {
                if (window())
                    window()->installEventFilter(this);
            },
            Qt::QueuedConnection);
    }
}

void TabPanelTitleBar::buildDockedButtons() {
    const int btnSize = 28;
    const QSize iconSize(16, 16);

    m_btnDetach = new Button;
    m_btnDetach->setObjectName("btnPanelDetach");
    m_btnDetach->setFixedSize(btnSize, btnSize);
    m_btnDetach->setIconSize(iconSize);
    m_btnDetach->setToolTip(tr("Detach to window"));
    m_btnDetach->installEventFilter(new ToolTipFilter(m_btnDetach, 500, false, true));
    connect(m_btnDetach, &Button::clicked, this, &TabPanelTitleBar::detachRequested);

    m_btnMaximize = new Button;
    m_btnMaximize->setObjectName("btnPanelMaximize");
    m_btnMaximize->setFixedSize(btnSize, btnSize);
    m_btnMaximize->setCheckable(true);
    m_btnMaximize->setIconSize(iconSize);
    m_btnMaximize->setToolTip(tr("Maximize or restore"));
    m_btnMaximize->installEventFilter(new ToolTipFilter(m_btnMaximize, 500, false, true));
    connect(m_btnMaximize, &Button::clicked, this, [=] {
        if (appStatus->trackPanelCollapsed)
            editorViewController->setPanelVisibility(true, true);
        else
            editorViewController->setPanelVisibility(false, true);
    });
    connect(appStatus, &AppStatus::trackPanelCollapseStateChanged, m_btnMaximize,
            &Button::setChecked);

    m_btnHide = new Button;
    m_btnHide->setObjectName("btnPanelHide");
    m_btnHide->setFixedSize(btnSize, btnSize);
    m_btnHide->setIconSize(iconSize);
    m_btnHide->setToolTip(tr("Hide"));
    m_btnHide->installEventFilter(new ToolTipFilter(m_btnHide, 500, false, true));
    connect(m_btnHide, &Button::clicked, this,
            [=] { editorViewController->setPanelVisibility(true, false); });

    rebuildIcons();

    m_btnLayout->addWidget(m_btnDetach);
    m_btnLayout->addWidget(m_btnMaximize);
    m_btnLayout->addWidget(m_btnHide);
}

void TabPanelTitleBar::rebuildIcons() {
    const QSize iconSize(16, 16);

    IconUtils::SvgIconColorPalette actionPalette;
    actionPalette.normal = m_iconColor;
    actionPalette.disabled = m_iconDisabledColor;

    IconUtils::SvgIconToggleColorPalette togglePalette;
    togglePalette.off = actionPalette;
    togglePalette.on.normal = m_iconOnColor;
    togglePalette.on.disabled = m_iconOnDisabledColor;

    if (m_btnDetach)
        m_btnDetach->setIcon(IconUtils::createTintedSvgIcon(
            ":svg/icons/panel_separate_window_20_filled.svg", iconSize, actionPalette));
    if (m_btnMaximize)
        m_btnMaximize->setIcon(IconUtils::createTintedSvgIcon(
            ":svg/icons/panel_maximize_24_filled.svg", iconSize, togglePalette));
    if (m_btnHide)
        m_btnHide->setIcon(IconUtils::createTintedSvgIcon(":svg/icons/panel_hide_24_filled.svg",
                                                          iconSize, actionPalette));
}

QColor TabPanelTitleBar::iconColor() const {
    return m_iconColor;
}

void TabPanelTitleBar::setIconColor(const QColor &color) {
    if (m_iconColor == color)
        return;
    m_iconColor = color;
    rebuildIcons();
    update();
}

QColor TabPanelTitleBar::iconDisabledColor() const {
    return m_iconDisabledColor;
}

void TabPanelTitleBar::setIconDisabledColor(const QColor &color) {
    if (m_iconDisabledColor == color)
        return;
    m_iconDisabledColor = color;
    rebuildIcons();
    update();
}

QColor TabPanelTitleBar::iconOnColor() const {
    return m_iconOnColor;
}

void TabPanelTitleBar::setIconOnColor(const QColor &color) {
    if (m_iconOnColor == color)
        return;
    m_iconOnColor = color;
    rebuildIcons();
    update();
}

QColor TabPanelTitleBar::iconOnDisabledColor() const {
    return m_iconOnDisabledColor;
}

void TabPanelTitleBar::setIconOnDisabledColor(const QColor &color) {
    if (m_iconOnDisabledColor == color)
        return;
    m_iconOnDisabledColor = color;
    rebuildIcons();
    update();
}

bool TabPanelTitleBar::eventFilter(QObject *watched, QEvent *event) {
    if (m_detached && watched == window()) {
        if (event->type() == QEvent::WindowActivate)
            setActiveStyle(true);
        else if (event->type() == QEvent::WindowDeactivate)
            setActiveStyle(false);
    }
    return QWidget::eventFilter(watched, event);
}

void TabPanelTitleBar::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
}

void TabPanelTitleBar::retranslateUi() {
    if (m_btnDetach)
        m_btnDetach->setToolTip(tr("Detach to window"));
    if (m_btnMaximize)
        m_btnMaximize->setToolTip(tr("Maximize or restore"));
    if (m_btnHide)
        m_btnHide->setToolTip(tr("Hide"));
}

void TabPanelTitleBar::afterSetAnimationEnabled(bool enabled) {
    Q_UNUSED(enabled)
    updateAnimationSettings();
}

void TabPanelTitleBar::afterSetTimeScale(double scale) {
    Q_UNUSED(scale)
    updateAnimationSettings();
}

void TabPanelTitleBar::setActiveStyle(bool active) {
    if (!m_detached || !m_opacityEffect)
        return;

    m_targetActive = active;
    m_animation->stop();
    m_animation->setStartValue(m_opacityEffect->opacity());
    const auto targetOpacity = active ? 1.0 : 0.5;
    const auto duration = getEffectiveAnimationTime(active ? 100 : 300);
    m_animation->setEndValue(targetOpacity);
    m_animation->setDuration(duration);
    if (duration == 0) {
        m_opacityEffect->setOpacity(targetOpacity);
        return;
    }
    m_animation->start();
}

void TabPanelTitleBar::updateAnimationSettings() {
    if (m_animation->state() == QAbstractAnimation::Running)
        setActiveStyle(m_targetActive);
}
