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

namespace {
    constexpr int kLeadingInset = 32;
    constexpr int kTrailingGap = 6;
} // namespace

MainTitleBar::MainTitleBar(MainMenuView *menuView, QWidget *parent,
                           const TitleBarLayoutPlan &plan)
    : QWidget(parent), m_window(parent), m_menuView(menuView), m_layoutPlan(plan) {
    setAttribute(Qt::WA_StyledBackground);

    m_playbackView = new PlaybackView(this);

    m_actionButtonsView = new ActionButtonsView(this);
    connect(m_actionButtonsView, &ActionButtonsView::saveTriggered, m_menuView->actionSave(),
            &QAction::trigger);

    m_titleComboBox = new TitleBarComboBox;

    if (m_layoutPlan.systemButtons) {
        int systemButtonWidth = 48;

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

    setLayout(new QHBoxLayout);
    // The menu bar section owns its own visibility: a native platform menu bar
    // is already hidden by Qt, and a hidden preview must not leave the widget
    // floating over the title bar at its default geometry.
    m_menuView->setInWindowMenu(m_layoutPlan.inWindowMenu);
    buildSections();

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

void MainTitleBar::setLayoutPlan(const TitleBarLayoutPlan &plan) {
    if (plan == m_layoutPlan)
        return;
    m_layoutPlan = plan;
    m_menuView->setInWindowMenu(m_layoutPlan.inWindowMenu);
    clearSections();
    buildSections();
    updateGeometry();
}

const TitleBarLayoutPlan &MainTitleBar::layoutPlan() const {
    return m_layoutPlan;
}

QList<DividerLine *> MainTitleBar::dividers() const {
    return m_dividers;
}

void MainTitleBar::clearSections() {
    auto *mainLayout = qobject_cast<QHBoxLayout *>(layout());
    if (!mainLayout)
        return;

    // Deleting the layout item never deletes its widget, so the permanent
    // sections survive; only the dividers and spacers are disposable.
    while (auto *item = mainLayout->takeAt(0))
        delete item;
    qDeleteAll(m_dividers);
    m_dividers.clear();
}

void MainTitleBar::buildSections() {
    auto *mainLayout = qobject_cast<QHBoxLayout *>(layout());
    if (!mainLayout)
        return;

    if (m_layoutPlan.appIconInset)
        mainLayout->addSpacerItem(new QSpacerItem(kLeadingInset, 20, QSizePolicy::Fixed));

    // The leading cluster is separated by dividers. A divider only exists when
    // the section before it is present, so removing the menu bar (native macOS
    // menu bar, hidden preview) can never leave a divider at the far left.
    bool hasLeadingSection = false;
    const auto addLeadingSection = [&](QWidget *section) {
        if (hasLeadingSection) {
            auto *divider = new DividerLine(Qt::Vertical);
            m_dividers.append(divider);
            mainLayout->addWidget(divider);
        }
        mainLayout->addWidget(section);
        hasLeadingSection = true;
    };

    if (m_layoutPlan.inWindowMenu)
        addLeadingSection(m_menuView);
    if (m_layoutPlan.projectSelector)
        addLeadingSection(m_titleComboBox);
    if (m_layoutPlan.documentActions)
        addLeadingSection(m_actionButtonsView);

    // The trailing cluster keeps the flex space in the middle and never gets a
    // divider: playback and the system buttons are not part of the leading run.
    mainLayout->addStretch();
    if (m_layoutPlan.playback)
        mainLayout->addWidget(m_playbackView);
    mainLayout->addSpacing(kTrailingGap);
    if (m_layoutPlan.systemButtons && m_btnMin) {
        mainLayout->addWidget(m_btnMin);
        mainLayout->addWidget(m_btnMax);
        mainLayout->addWidget(m_btnClose);
    }
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(m_layoutPlan.leftMargin, 0, 0, 0);
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
