#include <lite/GUI/Controls/SwitchButton.h>

#include <QPainter>
#include <QEvent>
#include <QPropertyAnimation>

SwitchButton::SwitchButton(QWidget *parent) : QAbstractButton(parent) {
    initUi();
}

SwitchButton::SwitchButton(const bool on, QWidget *parent) : QAbstractButton(parent) {
    m_apparentValue = on ? 255 : 0;
    initUi();
    setChecked(on);
}

SwitchButton::~SwitchButton() = default;

bool SwitchButton::value() const {
    return isChecked();
}

void SwitchButton::setValue(const bool value) {
    setChecked(value);
    m_valueAnimation.stop();
    m_valueAnimation.setStartValue(m_apparentValue);
    m_valueAnimation.setEndValue(isChecked() ? 255 : 0);
    m_valueAnimation.start();
}

void SwitchButton::initUi() {
    setCheckable(true);
    setAttribute(Qt::WA_Hover, true);
    installEventFilter(this);

    m_valueAnimation.setTargetObject(this);
    m_valueAnimation.setPropertyName("apparentValue");
    m_valueAnimation.setEasingCurve(QEasingCurve::OutBack);

    m_thumbHoverAnimation.setTargetObject(this);
    m_thumbHoverAnimation.setPropertyName("thumbScaleRatio");
    m_thumbHoverAnimation.setEasingCurve(QEasingCurve::OutCubic);

    setMinimumSize(40, 28);
    setMaximumSize(40, 28);
    connect(this, &QAbstractButton::clicked, this, &SwitchButton::setValue);

    initializeAnimation();
}

void SwitchButton::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen;


    // Calculate params
    const auto m_halfRectHeight = rect().height() / 2;
    const auto m_thumbRadius = (m_halfRectHeight - m_vPadding) / 2.0;
    QPointF m_trackStart;
    QPointF m_trackEnd;
    m_trackStart.setX(rect().left() + m_vPadding + m_thumbRadius + 1); // Avoid clipping
    m_trackStart.setY(m_halfRectHeight);
    m_trackEnd.setX(rect().right() - m_vPadding - m_thumbRadius);
    m_trackEnd.setY(m_halfRectHeight);
    const auto trackLength = m_trackEnd.x() - m_trackStart.x();

    // QSS qproperty cannot express :disabled, so pick the color set here
    const auto trackOffColor = isEnabled() ? m_trackOffColor : m_trackOffDisabledColor;
    const auto trackOnColor = isEnabled() ? m_trackOnColor : m_trackOnDisabledColor;
    const auto thumbOffColor = isEnabled() ? m_thumbOffColor : m_thumbOffDisabledColor;
    const auto thumbOnColor = isEnabled() ? m_thumbOnColor : m_thumbOnDisabledColor;

    // Draw inactive background
    pen.setWidthF(rect().height() - m_vPadding * 2);
    auto trackOff = trackOffColor;
    if (m_apparentValue == 255)
        trackOff.setAlpha(0);
    pen.setColor(trackOff);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.drawLine(m_trackStart, m_trackEnd);

    // Draw active background
    auto alpha = m_apparentValue;
    if (alpha > 255)
        alpha = 255;
    if (alpha < 0)
        alpha = 0;
    auto trackOn = trackOnColor;
    trackOn.setAlpha(alpha * trackOn.alpha() / 255);
    pen.setColor(trackOn);
    painter.setPen(pen);
    painter.drawLine(m_trackStart, m_trackEnd);

    // Draw thumb
    const auto left = m_apparentValue * trackLength / 255.0 + m_trackStart.x();
    const auto handlePos = QPointF(left, m_halfRectHeight);
    const auto thumbRadius = m_thumbRadius * m_thumbScaleRatio / 100.0;

    painter.setPen(Qt::NoPen);
    auto t = m_apparentValue;
    if (t > 255)
        t = 255;
    if (t < 0)
        t = 0;
    // Interpolate thumb color between off and on states (alpha included, or
    // translucent disabled tokens would render fully opaque)
    const auto lerp = [t](const int from, const int to) { return from + (to - from) * t / 255; };
    painter.setBrush(QColor(lerp(thumbOffColor.red(), thumbOnColor.red()),
                            lerp(thumbOffColor.green(), thumbOnColor.green()),
                            lerp(thumbOffColor.blue(), thumbOnColor.blue()),
                            lerp(thumbOffColor.alpha(), thumbOnColor.alpha())));
    painter.drawEllipse(handlePos, thumbRadius, thumbRadius);
}

int SwitchButton::apparentValue() const {
    return m_apparentValue;
}

void SwitchButton::setApparentValue(const int x) {
    m_apparentValue = x;
    repaint();
}

int SwitchButton::thumbScaleRatio() const {
    return m_thumbScaleRatio;
}

void SwitchButton::setThumbScaleRatio(const int ratio) {
    m_thumbScaleRatio = ratio;
    repaint();
}

QColor SwitchButton::trackOffColor() const {
    return m_trackOffColor;
}

void SwitchButton::setTrackOffColor(const QColor &color) {
    if (m_trackOffColor == color)
        return;
    m_trackOffColor = color;
    update();
}

QColor SwitchButton::trackOnColor() const {
    return m_trackOnColor;
}

void SwitchButton::setTrackOnColor(const QColor &color) {
    if (m_trackOnColor == color)
        return;
    m_trackOnColor = color;
    update();
}

QColor SwitchButton::thumbOffColor() const {
    return m_thumbOffColor;
}

void SwitchButton::setThumbOffColor(const QColor &color) {
    if (m_thumbOffColor == color)
        return;
    m_thumbOffColor = color;
    update();
}

QColor SwitchButton::thumbOnColor() const {
    return m_thumbOnColor;
}

void SwitchButton::setThumbOnColor(const QColor &color) {
    if (m_thumbOnColor == color)
        return;
    m_thumbOnColor = color;
    update();
}

QColor SwitchButton::trackOffDisabledColor() const {
    return m_trackOffDisabledColor;
}

void SwitchButton::setTrackOffDisabledColor(const QColor &color) {
    if (m_trackOffDisabledColor == color)
        return;
    m_trackOffDisabledColor = color;
    update();
}

QColor SwitchButton::trackOnDisabledColor() const {
    return m_trackOnDisabledColor;
}

void SwitchButton::setTrackOnDisabledColor(const QColor &color) {
    if (m_trackOnDisabledColor == color)
        return;
    m_trackOnDisabledColor = color;
    update();
}

QColor SwitchButton::thumbOffDisabledColor() const {
    return m_thumbOffDisabledColor;
}

void SwitchButton::setThumbOffDisabledColor(const QColor &color) {
    if (m_thumbOffDisabledColor == color)
        return;
    m_thumbOffDisabledColor = color;
    update();
}

QColor SwitchButton::thumbOnDisabledColor() const {
    return m_thumbOnDisabledColor;
}

void SwitchButton::setThumbOnDisabledColor(const QColor &color) {
    if (m_thumbOnDisabledColor == color)
        return;
    m_thumbOnDisabledColor = color;
    update();
}

void SwitchButton::updateAnimationDuration() {
    const auto valueDuration = getEffectiveAnimationTime(400);
    const auto hoverDuration = getEffectiveAnimationTime(200);

    auto updateAnimation = [](QPropertyAnimation &animation, int duration, int currentValue,
                              const auto &applyValue) {
        const auto running = animation.state() == QAbstractAnimation::Running;
        const auto endValue = animation.endValue().toInt();
        animation.stop();
        animation.setDuration(duration);
        if (!running)
            return;
        if (duration == 0) {
            applyValue(endValue);
            return;
        }
        animation.setStartValue(currentValue);
        animation.setEndValue(endValue);
        animation.start();
    };

    updateAnimation(m_valueAnimation, valueDuration, m_apparentValue,
                    [this](int value) { setApparentValue(value); });
    updateAnimation(m_thumbHoverAnimation, hoverDuration, m_thumbScaleRatio,
                    [this](int value) { setThumbScaleRatio(value); });
}

bool SwitchButton::eventFilter(QObject *object, QEvent *event) {
    const auto type = event->type();
    if (type == QEvent::EnabledChange) {
        // HoverLeave never arrives once disabled, shrink the thumb back smoothly
        m_thumbHoverAnimation.stop();
        m_thumbHoverAnimation.setStartValue(m_thumbScaleRatio);
        m_thumbHoverAnimation.setEndValue(100);
        m_thumbHoverAnimation.start();
        update();
    } else if (isEnabled()) {
        if (type == QEvent::HoverEnter || type == QEvent::MouseButtonRelease) {
            m_thumbHoverAnimation.stop();
            m_thumbHoverAnimation.setStartValue(m_thumbScaleRatio);
            m_thumbHoverAnimation.setEndValue(125);
            m_thumbHoverAnimation.start();
        } else if (type == QEvent::HoverLeave) {
            m_thumbHoverAnimation.stop();
            m_thumbHoverAnimation.setStartValue(m_thumbScaleRatio);
            m_thumbHoverAnimation.setEndValue(100);
            m_thumbHoverAnimation.start();
        } else if (type == QEvent::MouseButtonPress) {
            m_thumbHoverAnimation.stop();
            m_thumbHoverAnimation.setStartValue(m_thumbScaleRatio);
            m_thumbHoverAnimation.setEndValue(85);
            m_thumbHoverAnimation.start();
        }
    }
    return QObject::eventFilter(object, event);
}

void SwitchButton::afterSetAnimationEnabled(bool enabled) {
    updateAnimationDuration();
}

void SwitchButton::afterSetTimeScale(double scale) {
    updateAnimationDuration();
}
