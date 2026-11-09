#include "MainTitleBar.h"

#include "ActionButtonsView.h"
#include <lite/GUI/Controls/DividerLine.h>
#include "MainMenuView.h"
#include "PlaybackView.h"
#include "TitleBarComboBox.h"
#include <lite/GUI/Controls/SystemWindowButton.h>
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

MainTitleBar::MainTitleBar(MainMenuView *menuView, QWidget *parent,
                           const TitleBarSections &sections, const TitleBarRole role)
    : QWidget(parent), m_window(parent), m_menuView(menuView), m_sections(sections) {
    setAttribute(Qt::WA_StyledBackground);

    if (sections.playback)
        m_playbackView = new PlaybackView(this, role);
    if (sections.projectSelector)
        m_titleComboBox = new TitleBarComboBox;
    if (sections.documentActions) {
        m_actionButtonsView = new ActionButtonsView(this);
        if (menuView)
            connect(m_actionButtonsView, &ActionButtonsView::saveTriggered,
                    menuView->actionSave(), &QAction::trigger);
    }
    if (sections.systemButtons)
        createSystemButtons();

    buildSections();
    setFixedHeight(40);

    m_opacityEffect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(m_opacityEffect);
    m_animation = new QVariantAnimation(this);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) { m_opacityEffect->setOpacity(value.toDouble()); });
    initializeAnimation();

    // The bar follows its own window instead of relying on the caller to wire
    // the filter up. This is what makes the same chrome reusable as-is.
    if (m_window)
        m_window->installEventFilter(this);

    // Re-tint system button icons when the theme changes (non-Windows only)
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, [this](const QString &) {
        if (!SystemUtils::isWindows())
            rebuildSystemButtonIcons();
    });
}

void MainTitleBar::createSystemButtons() {
    constexpr int systemButtonWidth = 48;

    m_btnMin = new SystemWindowButton;
    m_btnMin->setObjectName("MinimizeButton");
    m_btnMin->setFixedSize(systemButtonWidth, 40);

    m_btnMax = new SystemWindowButton;
    m_btnMax->setCheckable(true);
    m_btnMax->setObjectName("MaximizeButton");
    m_btnMax->setFixedSize(systemButtonWidth, 40);

    m_btnClose = new SystemWindowButton;
    m_btnClose->setCloseStyle(true);
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

    connect(m_btnMin, &SystemWindowButton::clicked, this, &MainTitleBar::minimizeTriggered);
    connect(m_btnMax, &SystemWindowButton::clicked, this, &MainTitleBar::maximizeTriggered);
    connect(m_btnClose, &SystemWindowButton::clicked, this, &MainTitleBar::closeTriggered);
}

void MainTitleBar::addLeadingDivider() {
    // Only adjacent leading sections are separated. A reduced plan therefore
    // never leaves a divider as the first visible element of the title bar.
    if (m_leadingSectionCount > 0)
        m_layout->addWidget(new DividerLine(Qt::Vertical));
    ++m_leadingSectionCount;
}

void MainTitleBar::buildSections() {
    m_layout = new QHBoxLayout;
    m_leadingSectionCount = 0;

    if (m_sections.appIconInset) {
        // TODO: app icon
        m_layout->addSpacerItem(new QSpacerItem(32, 20, QSizePolicy::Fixed));
    }
    if (m_sections.mainMenu && m_menuView) {
        addLeadingDivider();
        auto *menuBarContainer = new QHBoxLayout;
        menuBarContainer->addWidget(m_menuView);
        menuBarContainer->setContentsMargins({});
        m_layout->addLayout(menuBarContainer);
    }
    if (m_sections.projectSelector && m_titleComboBox) {
        addLeadingDivider();
        m_layout->addWidget(m_titleComboBox);
    }
    if (m_sections.documentActions && m_actionButtonsView) {
        addLeadingDivider();
        m_layout->addWidget(m_actionButtonsView);
    }

    m_layout->addStretch();
    if (m_playbackView)
        m_layout->addWidget(m_playbackView);
    m_layout->addSpacing(6);
    if (m_btnMin) {
        m_layout->addWidget(m_btnMin);
        m_layout->addWidget(m_btnMax);
        m_layout->addWidget(m_btnClose);
    }
    m_layout->setSpacing(0);
    m_layout->setContentsMargins(m_sections.leftMargin, 0, 0, 0);
    setLayout(m_layout);
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

SystemWindowButton *MainTitleBar::minimizeButton() const {
    return m_btnMin;
}

SystemWindowButton *MainTitleBar::maximizeButton() const {
    return m_btnMax;
}

SystemWindowButton *MainTitleBar::closeButton() const {
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

SystemWindowButton *MainTitleBar::systemButtonAt(const QPointF &windowPos) const {
    // Check from the rightmost button so that positions beyond the close
    // button's left edge (the window's last logical pixel column) still
    // resolve to the close button.
    for (const auto btn : {m_btnClose, m_btnMax, m_btnMin}) {
        if (!btn || btn->isHidden())
            continue;
        const QRect geo(btn->mapTo(m_window, QPoint(0, 0)), btn->size());
        if (windowPos.y() >= geo.top() && windowPos.y() < geo.bottom() &&
            windowPos.x() >= geo.left())
            return btn;
    }
    return nullptr;
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
