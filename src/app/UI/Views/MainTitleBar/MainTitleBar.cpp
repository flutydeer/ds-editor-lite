#include "MainTitleBar.h"

#include "ActionButtonsView.h"
#include <lite/GUI/Controls/DividerLine.h>
#include "MainMenuView.h"
#include "PlaybackView.h"
#include "TitleBarComboBox.h"
#include "Controller/AppController.h"
#include <lite/History/HistoryManager.h>
#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/ToolTipFilter.h>
#include <lite/GUI/Theme/ThemeManager.h>
#include <lite/GUI/Utils/IconUtils.h>
#include <lite/Support/SystemUtils.h>

#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QVariantAnimation>

#define ChromeMinimize QStringLiteral(u"\ue921")
#define ChromeMaximize QStringLiteral(u"\ue922")
#define ChromeRestore  QStringLiteral(u"\ue923")
#define ChromeClose    QStringLiteral(u"\ue8bb")

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

    m_titleComboBox = new TitleBarComboBox;

    if (!useNativeFrame) {
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

            m_btnMin->setText(ChromeMinimize);
            m_btnMin->setFont(font);
            m_btnMax->setText(ChromeMaximize);
            m_btnMax->setFont(font);
            m_btnClose->setText(ChromeClose);
            m_btnClose->setFont(font);
        } else {
            rebuildSystemButtonIcons();
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
    auto dividerBeforeTitle = new DividerLine(Qt::Vertical);
    mainLayout->addWidget(dividerBeforeTitle);
    mainLayout->addWidget(m_titleComboBox);
    auto dividerBeforeTools = new DividerLine(Qt::Vertical);
    mainLayout->addWidget(dividerBeforeTools);
    mainLayout->addWidget(m_actionButtonsView);
    mainLayout->addStretch();
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
            [this](const QVariant &value) { m_opacityEffect->setOpacity(value.toDouble()); });
    initializeAnimation();

    // Re-tint system button icons when the theme changes (non-Windows only)
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, [this](const QString &) {
        if (!SystemUtils::isWindows())
            rebuildSystemButtonIcons();
    });
}

void MainTitleBar::rebuildSystemButtonIcons() {
    if (!m_btnMin || !m_btnMax || !m_btnClose)
        return;

    constexpr auto icoSize = QSize(14, 14);
    const auto iconColor = ThemeManager::instance()->semanticColor(QStringLiteral("icon.primary"));
    const auto disabledColor =
        ThemeManager::instance()->semanticColor(QStringLiteral("icon.disabled"));

    IconUtils::SvgIconColorPalette palette;
    palette.normal = iconColor.isValid() ? iconColor : QColor(240, 240, 240);
    palette.disabled = disabledColor.isValid() ? disabledColor : QColor(240, 240, 240, 102);
    palette.active = palette.normal;
    palette.selected = palette.normal;

    m_btnMin->setIconSize(icoSize);
    m_btnMin->setIcon(IconUtils::createTintedSvgIcon(":svg/title-bar/minimize_16_filled_white.svg",
                                                     icoSize, palette));

    const bool maximized = m_window && m_window->isMaximized();
    const auto maximizePath = maximized
                                  ? QStringLiteral(":svg/title-bar/restore_16_filled_white.svg")
                                  : QStringLiteral(":svg/title-bar/maximize_16_filled_white.svg");
    m_btnMax->setIconSize(icoSize);
    m_btnMax->setIcon(IconUtils::createTintedSvgIcon(maximizePath, icoSize, palette));

    m_btnClose->setIconSize(icoSize);
    m_btnClose->setIcon(IconUtils::createTintedSvgIcon(":svg/title-bar/close_16_filled_white.svg",
                                                       icoSize, palette));
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

TitleBarComboBox *MainTitleBar::titleComboBox() const {
    return m_titleComboBox;
}

void MainTitleBar::setTitle(const QString &title) const {
    if (!m_titleComboBox)
        return;
    m_titleComboBox->setTitle(title);
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
                m_btnMax->setText(checked ? ChromeRestore : ChromeMaximize);
            else
                rebuildSystemButtonIcons();
        }
    } else if (event->type() == QEvent::WindowActivate)
        setActiveStyle(true);
    else if (event->type() == QEvent::WindowDeactivate)
        setActiveStyle(false);

    return QWidget::eventFilter(watched, event);
}

void MainTitleBar::afterSetAnimationEnabled(bool enabled) {
    Q_UNUSED(enabled)
    updateAnimationSettings();
}

void MainTitleBar::afterSetTimeScale(double scale) {
    Q_UNUSED(scale)
    updateAnimationSettings();
}

void MainTitleBar::setActiveStyle(bool active) {
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

void MainTitleBar::updateAnimationSettings() {
    if (m_animation->state() == QAbstractAnimation::Running)
        setActiveStyle(m_targetActive);
}
