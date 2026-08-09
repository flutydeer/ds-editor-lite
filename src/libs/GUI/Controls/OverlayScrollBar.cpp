#include <lite/GUI/Controls/OverlayScrollBar.h>

#include <QAbstractScrollArea>
#include <QEvent>
#include <QPainter>
#include <QScrollBar>
#include <QVariantAnimation>

static constexpr int kBarThickness = 16;
static constexpr int kHandleMargin = 4;
static constexpr int kHandleMinLength = 20;

OverlayScrollBar::OverlayScrollBar(Qt::Orientation orientation, QWidget *parent)
    : QScrollBar(orientation, parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_Hover);
    if (orientation == Qt::Horizontal)
        setFixedHeight(kBarThickness);
    else
        setFixedWidth(kBarThickness);

    m_animation = new QVariantAnimation(this);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_opacity = value.toDouble();
        update();
    });

    m_geometryAnimation = new QVariantAnimation(this);
    m_geometryAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_geometryAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
                m_geometryProgress = value.toDouble();
                update();
            });

    // initializeAnimation 会同步回调 afterSetAnimationLevel/afterSetTimeScale，
    // 必须在两个动画对象创建之后调用
    initializeAnimation();

    connect(this, &QScrollBar::sliderPressed, this, [this] {
        m_pressed = true;
        updateVisualState();
    });
    connect(this, &QScrollBar::sliderReleased, this, [this] {
        m_pressed = false;
        updateVisualState();
    });
}

void OverlayScrollBar::attachTo(QAbstractScrollArea *scrollArea) {
    m_scrollArea = scrollArea;

    const bool horizontal = orientation() == Qt::Horizontal;
    if (horizontal)
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    else
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *source = horizontal ? scrollArea->horizontalScrollBar() : scrollArea->verticalScrollBar();

    connect(source, &QScrollBar::rangeChanged, this, [this](int min, int max) {
        setRange(min, max);
        setVisible(max > 0);
        updatePosition();
    });
    connect(source, &QScrollBar::valueChanged, this, &QScrollBar::setValue);
    connect(this, &QScrollBar::valueChanged, source, &QScrollBar::setValue);

    setRange(source->minimum(), source->maximum());
    setPageStep(source->pageStep());
    setSingleStep(source->singleStep());
    setVisible(source->maximum() > 0);

    connect(source, &QScrollBar::rangeChanged, this, [this, source] {
        setPageStep(source->pageStep());
        setSingleStep(source->singleStep());
    });

    scrollArea->viewport()->installEventFilter(this);
}

void OverlayScrollBar::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)

    if (maximum() <= 0)
        return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int totalRange = maximum() - minimum() + pageStep();
    if (totalRange <= 0)
        return;

    const bool horizontal = orientation() == Qt::Horizontal;
    int availableLength = (horizontal ? width() : height()) - 2 * kHandleMargin;
    int handleLength = qMax(kHandleMinLength, availableLength * pageStep() / totalRange);
    int handlePos = kHandleMargin;
    if (maximum() > minimum())
        handlePos +=
            (availableLength - handleLength) * (value() - minimum()) / (maximum() - minimum());

    qreal baseOpacity = 0.25 + 0.10 * m_opacity;
    auto color = m_handleColor;
    color.setAlpha(qRound(color.alpha() * baseOpacity));
    p.setBrush(color);
    p.setPen(Qt::NoPen);

    // 条厚 16px：平时手柄 2px 贴外缘（距边 4px），hover/按住时外缘边不动、
    // 向内侧展开到 6px 宽（圆角 1→2）；m_geometryProgress 0→1 插值，
    // 与透明度动画使用相同的时长与曲线
    const qreal t = m_geometryProgress;
    const qreal radius = 1.0 + t;

    if (horizontal) {
        const qreal y = height() - 6.0 - 4.0 * t;
        const qreal h = 2.0 + 4.0 * t;
        p.drawRoundedRect(QRectF(handlePos, y, handleLength, h), radius, radius);
    } else {
        const qreal x = width() - 6.0 - 4.0 * t;
        const qreal w = 2.0 + 4.0 * t;
        p.drawRoundedRect(QRectF(x, handlePos, w, handleLength), radius, radius);
    }
}

void OverlayScrollBar::enterEvent(QEnterEvent *event) {
    Q_UNUSED(event)
    m_hovered = true;
    updateVisualState();
}

void OverlayScrollBar::leaveEvent(QEvent *event) {
    Q_UNUSED(event)
    m_hovered = false;
    updateVisualState();
}

bool OverlayScrollBar::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_scrollArea->viewport() && event->type() == QEvent::Resize)
        updatePosition();
    return QScrollBar::eventFilter(watched, event);
}

void OverlayScrollBar::setHighlightVisible(bool visible) {
    m_targetHighlightVisible = visible;
    m_animation->stop();
    m_animation->setStartValue(m_opacity);
    const auto targetOpacity = visible ? 1.0 : 0.0;
    const auto duration = getEffectiveAnimationTime(visible ? 100 : 300);
    m_animation->setEndValue(targetOpacity);
    m_animation->setDuration(duration);
    if (duration == 0) {
        m_opacity = targetOpacity;
        update();
        return;
    }
    m_animation->start();
}

void OverlayScrollBar::updateVisualState() {
    const bool active = m_hovered || m_pressed;
    m_targetGeometryVisible = active;
    setHighlightVisible(active);
    updateGeometryAnimation();
}

void OverlayScrollBar::updateGeometryAnimation() {
    const auto target = m_targetGeometryVisible ? 1.0 : 0.0;
    m_geometryAnimation->stop();
    m_geometryAnimation->setStartValue(m_geometryProgress);
    m_geometryAnimation->setEndValue(target);
    const auto duration = getEffectiveAnimationTime(m_targetGeometryVisible ? 100 : 300);
    m_geometryAnimation->setDuration(duration);
    if (duration == 0) {
        m_geometryProgress = target;
        update();
        return;
    }
    m_geometryAnimation->start();
}

void OverlayScrollBar::afterSetAnimationLevel(AnimationGlobal::AnimationLevels level) {
    Q_UNUSED(level)
    updateAnimationSettings();
}

void OverlayScrollBar::afterSetTimeScale(double scale) {
    Q_UNUSED(scale)
    updateAnimationSettings();
}

void OverlayScrollBar::updateAnimationSettings() {
    if (m_animation->state() == QAbstractAnimation::Running)
        setHighlightVisible(m_targetHighlightVisible);
    if (m_geometryAnimation->state() == QAbstractAnimation::Running)
        updateGeometryAnimation();
}

QColor OverlayScrollBar::handleColor() const {
    return m_handleColor;
}

void OverlayScrollBar::setHandleColor(const QColor &color) {
    if (m_handleColor == color)
        return;
    m_handleColor = color;
    update();
}

void OverlayScrollBar::updatePosition() {
    if (!m_scrollArea)
        return;

    auto viewport = m_scrollArea->viewport();
    auto mapped = viewport->mapTo(parentWidget(), QPoint(0, 0));
    if (orientation() == Qt::Horizontal) {
        setGeometry(mapped.x(), mapped.y() + viewport->height() - kBarThickness, viewport->width(),
                    kBarThickness);
    } else {
        setGeometry(mapped.x() + viewport->width() - kBarThickness, mapped.y(), kBarThickness,
                    viewport->height());
    }
    raise();
}

OverlayScrollBar *OverlayScrollBar::install(QAbstractScrollArea *scrollArea,
                                            Qt::Orientation orientation) {
    auto *bar = new OverlayScrollBar(orientation, scrollArea);
    bar->attachTo(scrollArea);
    bar->updatePosition();
    return bar;
}
