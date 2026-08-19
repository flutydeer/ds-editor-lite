#include "DialogTitleBar.h"

#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Theme/ThemeManager.h>
#include <lite/GUI/Utils/IconUtils.h>
#include <lite/Support/SystemUtils.h>

#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QSysInfo>
#include <QVariantAnimation>

#define ChromeClose QStringLiteral(u"\ue8bb")

DialogTitleBar::DialogTitleBar(QWidget *parent)
    : QWidget(parent), m_window(parent ? parent->window() : nullptr) {
    setAttribute(Qt::WA_StyledBackground);

    m_lbTitle = new QLabel;
    m_lbTitle->setObjectName("DialogTitleBarLabel");

    int systemButtonWidth = 48;

    m_btnClose = new Button;
    m_btnClose->setObjectName("CloseButton");
    m_btnClose->setFixedSize(systemButtonWidth, 32);

    if (SystemUtils::isWindows()) {
        auto fontFamily =
            QSysInfo::productVersion() == "11" ? "Segoe Fluent Icons" : "Segoe MDL2 Assets";
        auto font = QFont(fontFamily);
        font.setPointSizeF(7.2);
        m_btnClose->setText(ChromeClose);
        m_btnClose->setFont(font);
    } else {
        rebuildCloseButtonIcon();
    }

    connect(m_btnClose, &Button::clicked, this, &DialogTitleBar::closeTriggered);

    auto mainLayout = new QHBoxLayout;
    mainLayout->addSpacing(16);
    mainLayout->addWidget(m_lbTitle, 1);
    mainLayout->addWidget(m_btnClose);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    setLayout(mainLayout);
    setFixedHeight(32);

    m_opacityEffect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(m_opacityEffect);
    m_animation = new QVariantAnimation(this);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) { m_opacityEffect->setOpacity(value.toDouble()); });
    initializeAnimation();

    // Re-tint the close icon when the theme changes (non-Windows only)
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this,
            [this](const QString &) {
                if (!SystemUtils::isWindows())
                    rebuildCloseButtonIcon();
            });

    if (m_window)
        m_window->installEventFilter(this);
}

void DialogTitleBar::rebuildCloseButtonIcon() {
    if (!m_btnClose)
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

    m_btnClose->setIconSize(icoSize);
    m_btnClose->setIcon(
        IconUtils::createTintedSvgIcon(":svg/title-bar/close_16_filled_white.svg", icoSize, palette));
}

Button *DialogTitleBar::closeButton() const {
    return m_btnClose;
}

void DialogTitleBar::setTitle(const QString &title) const {
    m_lbTitle->setText(title);
}

bool DialogTitleBar::eventFilter(QObject *watched, QEvent *event) {
    if (watched != m_window)
        return QWidget::eventFilter(watched, event);

    if (event->type() == QEvent::WindowTitleChange) {
        setTitle(m_window->windowTitle());
    } else if (event->type() == QEvent::WindowActivate) {
        setActiveStyle(true);
    } else if (event->type() == QEvent::WindowDeactivate) {
        setActiveStyle(false);
    }

    return QWidget::eventFilter(watched, event);
}

void DialogTitleBar::afterSetAnimationEnabled(bool enabled) {
    Q_UNUSED(enabled)
    updateAnimationSettings();
}

void DialogTitleBar::afterSetTimeScale(double scale) {
    Q_UNUSED(scale)
    updateAnimationSettings();
}

void DialogTitleBar::setActiveStyle(bool active) {
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

void DialogTitleBar::updateAnimationSettings() {
    if (m_animation->state() == QAbstractAnimation::Running)
        setActiveStyle(m_targetActive);
}
