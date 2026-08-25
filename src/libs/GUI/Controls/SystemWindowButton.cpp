#include <lite/GUI/Controls/SystemWindowButton.h>
#include <lite/GUI/Theme/ThemeManager.h>

#include <QPainter>
#include <QVariantAnimation>

SystemWindowButton::SystemWindowButton(QWidget *parent) : Button(parent) {
    // QSS keeps `background: transparent` and removes the `:hover`/`:pressed`
    // background rules; the animated fill is painted here.
    setAttribute(Qt::WA_Hover);

    m_animation = new QVariantAnimation(this);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_progress = value.toDouble();
        update();
    });

    connect(this, &QAbstractButton::pressed, this, [this] {
        m_pressed = true;
        updateVisualState();
    });
    connect(this, &QAbstractButton::released, this, [this] {
        m_pressed = false;
        updateVisualState();
    });

    refreshColors();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this,
            &SystemWindowButton::refreshColors);

    // initializeAnimation must run after the animation object is created,
    // the same requirement as OverlayScrollBar.
    initializeAnimation();
}

void SystemWindowButton::paintEvent(QPaintEvent *event) {
    if (m_progress > 0.0) {
        QPainter p(this);
        const QColor target = m_pressed ? m_pressedColor : m_hoverColor;
        QColor fill = target;
        fill.setAlpha(qRound(fill.alpha() * m_progress));
        p.fillRect(rect(), fill);
    }
    Button::paintEvent(event);
}

void SystemWindowButton::enterEvent(QEnterEvent *event) {
    m_hovered = true;
    Button::enterEvent(event);
    updateVisualState();
}

void SystemWindowButton::leaveEvent(QEvent *event) {
    m_hovered = false;
    Button::leaveEvent(event);
    updateVisualState();
}

void SystemWindowButton::afterSetAnimationEnabled(bool enabled) {
    Q_UNUSED(enabled)
    if (m_animation->state() == QAbstractAnimation::Running)
        updateVisualState();
}

void SystemWindowButton::afterSetTimeScale(double scale) {
    Q_UNUSED(scale)
    if (m_animation->state() == QAbstractAnimation::Running)
        updateVisualState();
}

void SystemWindowButton::updateVisualState() {
    animateTo(m_hovered || m_pressed ? 1.0 : 0.0);
}

void SystemWindowButton::animateTo(qreal target) {
    m_animation->stop();
    m_animation->setStartValue(m_progress);
    m_animation->setEndValue(target);
    const auto duration = getEffectiveAnimationTime(target != 0.0 ? 100 : 300);
    m_animation->setDuration(duration);
    if (duration == 0) {
        m_progress = target;
        update();
        return;
    }
    m_animation->start();
}

void SystemWindowButton::refreshColors() {
    ThemeManager *themeManager = ThemeManager::instance();
    const auto hoverToken =
        m_close ? QStringLiteral("window.closeButton.hover") : QStringLiteral("control.fill.hover");
    const auto pressedToken = m_close ? QStringLiteral("window.closeButton.pressed")
                                      : QStringLiteral("control.fill.pressed");
    m_hoverColor = themeManager->semanticColor(hoverToken);
    m_pressedColor = themeManager->semanticColor(pressedToken);
    update();
}

void SystemWindowButton::setCloseStyle(bool close) {
    if (m_close == close)
        return;
    m_close = close;
    refreshColors();
}
