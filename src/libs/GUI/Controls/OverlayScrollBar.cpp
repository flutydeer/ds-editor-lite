#include <lite/GUI/Controls/OverlayScrollBar.h>

#include <QAbstractScrollArea>
#include <QCursor>
#include <QEvent>
#include <QPainter>
#include <QPoint>
#include <QScrollBar>
#include <QTimer>
#include <QVariantAnimation>

static constexpr int kBarThickness = 16;
static constexpr int kHandleMargin = 4;
static constexpr int kHandleMinLength = 20;
static constexpr int kHideDelayMs = 3000;

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

    m_visibilityAnimation = new QVariantAnimation(this);
    m_visibilityAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_visibilityAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
                m_visibility = value.toDouble();
                update();
            });

    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    m_hideTimer->setInterval(kHideDelayMs);
    connect(m_hideTimer, &QTimer::timeout, this, &OverlayScrollBar::onHideTimeout);

    // 无按键的 MouseMove 只会发给视口下最深层的 widget（且需开启鼠标跟踪），
    // 视口内的子控件可能吞掉事件，故改用轮询全局鼠标位置
    m_cursorPollTimer = new QTimer(this);
    m_cursorPollTimer->setInterval(200);
    connect(m_cursorPollTimer, &QTimer::timeout, this, &OverlayScrollBar::pollCursor);
    m_cursorPollTimer->start();

    // initializeAnimation 会同步回调 afterSetAnimationLevel/afterSetTimeScale，
    // 必须在动画对象创建之后调用
    initializeAnimation();

    connect(this, &QScrollBar::sliderPressed, this, [this] {
        m_pressed = true;
        updateVisualState();
    });
    connect(this, &QScrollBar::sliderReleased, this, [this] {
        m_pressed = false;
        updateVisualState();
        if (!m_hovered)
            m_hideTimer->start();
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

    // Qt's QGraphicsViewPrivate::recalculateContentSize() calls setRange(),
    // which emits rangeChanged synchronously, and only AFTER that emission
    // assigns the new pageStep/singleStep (which emit no signal of their own).
    // A direct handler reading source->pageStep() inside rangeChanged would
    // therefore always latch the PREVIOUS pageStep - the QScrollBar default 10
    // on the very first recompute, collapsing the handle to minimum length and
    // leaving it at mid-track (the start-of-app scrollbar bug; only the next
    // range change, e.g. a zoom, fixed it). Copy the steps with a queued
    // connection so the read happens after the emitter's stack unwinds.
    connect(source, &QScrollBar::rangeChanged, this, [this](int min, int max) {
        setRange(min, max);
        setVisible(m_rangeVisible && max > 0);
        updatePosition();
    });
    connect(
        source, &QScrollBar::rangeChanged, this,
        [this, source](int, int) {
            setPageStep(source->pageStep());
            setSingleStep(source->singleStep());
        },
        Qt::QueuedConnection);
    connect(source, &QScrollBar::valueChanged, this, &QScrollBar::setValue);
    connect(source, &QScrollBar::valueChanged, this, &OverlayScrollBar::restartHideTimer);
    connect(this, &QScrollBar::valueChanged, source, &QScrollBar::setValue);

    setRange(source->minimum(), source->maximum());
    setPageStep(source->pageStep());
    setSingleStep(source->singleStep());
    setVisible(m_rangeVisible && source->maximum() > 0);

    // 视口需开启鼠标跟踪，未按键的 MouseMove 才会到达 eventFilter，
    // 否则鼠标在视口内移动无法重置自动隐藏计时
    auto *viewport = scrollArea->viewport();
    viewport->setMouseTracking(true);
    viewport->installEventFilter(this);
    m_lastCursorPos = QCursor::pos();
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

    // 可见性因子：m_visibility 0→1 淡入，静止 3s 后淡出隐藏（与高亮/几何动画同曲线）
    qreal baseOpacity = (0.25 + 0.10 * m_opacity) * m_visibility;
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
    if (!m_pressed)
        m_hideTimer->start();
}

bool OverlayScrollBar::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_geometryHost && event->type() == QEvent::Resize) {
        updatePosition();
    } else if (watched == m_scrollArea->viewport()) {
        if (event->type() == QEvent::Resize)
            updatePosition();
        else if (event->type() == QEvent::Enter || event->type() == QEvent::MouseMove)
            restartHideTimer();
    }
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
    updateVisibilityAnimation();
    if (active)
        m_hideTimer->stop();
}

void OverlayScrollBar::updateVisibilityAnimation() {
    const auto target = (m_hovered || m_pressed || m_idleVisible) ? 1.0 : 0.0;
    if (qFuzzyCompare(m_visibility, target)) {
        m_visibility = target;
        update();
        return;
    }
    m_visibilityAnimation->stop();
    m_visibilityAnimation->setStartValue(m_visibility);
    m_visibilityAnimation->setEndValue(target);
    const auto duration = getEffectiveAnimationTime(target != 0.0 ? 100 : 500);
    m_visibilityAnimation->setDuration(duration);
    if (duration == 0) {
        m_visibility = target;
        update();
        return;
    }
    m_visibilityAnimation->start();
}

void OverlayScrollBar::restartHideTimer() {
    m_idleVisible = true;
    updateVisibilityAnimation();
    if (m_hovered || m_pressed)
        m_hideTimer->stop();
    else
        m_hideTimer->start();
}

void OverlayScrollBar::onHideTimeout() {
    if (m_hovered || m_pressed)
        return;
    m_idleVisible = false;
    updateVisibilityAnimation();
}

void OverlayScrollBar::pollCursor() {
    if (!m_scrollArea || !m_scrollArea->isVisible())
        return;
    const QPoint pos = QCursor::pos();
    if (pos == m_lastCursorPos)
        return;
    m_lastCursorPos = pos;
    auto *viewport = m_scrollArea->viewport();
    if (viewport->rect().contains(viewport->mapFromGlobal(pos)))
        restartHideTimer();
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
    if (m_visibilityAnimation->state() == QAbstractAnimation::Running)
        updateVisibilityAnimation();
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

void OverlayScrollBar::setRangeVisible(bool visible) {
    m_rangeVisible = visible;
    setVisible(visible && maximum() > 0);
    updatePosition();
}

void OverlayScrollBar::setCompanion(OverlayScrollBar *companion) {
    if (m_companion == companion)
        return;
    m_companion = companion;
    updatePosition();
}

void OverlayScrollBar::setGeometryHost(QWidget *host) {
    if (m_geometryHost == host && parentWidget() == host)
        return;
    if (m_geometryHost)
        m_geometryHost->removeEventFilter(this);
    m_geometryHost = host;
    setParent(host);
    if (host) {
        host->installEventFilter(this);
        updatePosition();
    }
}

bool OverlayScrollBar::willShow() const {
    return m_rangeVisible && maximum() > 0;
}

void OverlayScrollBar::updatePosition() {
    // Lay out the companion bar first, then this one, so this bar reads the
    // companion's freshest visibility state.
    if (m_companion)
        m_companion->updateLayout();
    updateLayout();
}

void OverlayScrollBar::updateLayout() {
    if (!m_scrollArea)
        return;

    auto viewport = m_scrollArea->viewport();
    auto mapped = viewport->mapTo(parentWidget(), QPoint(0, 0));
    const bool companionShown = m_companion && m_companion->willShow();
    if (orientation() == Qt::Horizontal) {
        const int width = companionShown ? viewport->width() - kBarThickness : viewport->width();
        setGeometry(mapped.x(), mapped.y() + viewport->height() - kBarThickness, width,
                    kBarThickness);
    } else {
        const int height = companionShown ? viewport->height() - kBarThickness : viewport->height();
        setGeometry(mapped.x() + viewport->width() - kBarThickness, mapped.y(), kBarThickness,
                    height);
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
