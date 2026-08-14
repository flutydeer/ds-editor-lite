#include "PlaybackIndicatorOverlay.h"

#include <QEvent>
#include <QPainter>

#include <cmath>

PlaybackIndicatorOverlay::PlaybackIndicatorOverlay(const Shape shape, QWidget *parent)
    : QWidget(parent), m_shape(shape) {
    setObjectName(QStringLiteral("playbackIndicatorOverlay"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);
    setAutoFillBackground(false);
    if (parent) {
        parent->installEventFilter(this);
    }
    show();
    raise();
    updateGeometry();
}

PlaybackIndicatorOverlay::~PlaybackIndicatorOverlay() = default;

void PlaybackIndicatorOverlay::setPosition(const qreal x) {
    if (m_position == x)
        return;
    const auto oldPosition = m_position;
    const auto oldPositionVisible = isPositionVisible(oldPosition);
    const auto oldRect = indicatorRect(oldPosition);
    m_position = x;
    const auto positionVisible = isPositionVisible(m_position);
    const auto crossesViewport = oldPositionVisible && positionVisible && width() > 0 &&
                                 std::abs(m_position - oldPosition) > width() / 2.0;
    update(oldRect.united(indicatorRect(m_position)));
    if ((!oldPositionVisible && positionVisible) || crossesViewport)
        scheduleRefresh();
}

void PlaybackIndicatorOverlay::setColor(const QColor &color) {
    if (m_color == color)
        return;
    m_color = color;
    update(indicatorRect(m_position));
}

void PlaybackIndicatorOverlay::setIndicatorVisible(const bool visible) {
    if (m_indicatorVisible == visible)
        return;
    const auto dirtyRect = indicatorRect(m_position);
    m_indicatorVisible = visible;
    update(dirtyRect);
}

bool PlaybackIndicatorOverlay::eventFilter(QObject *watched, QEvent *event) {
    if (watched == parentWidget() &&
        (event->type() == QEvent::Resize || event->type() == QEvent::Show ||
         event->type() == QEvent::DevicePixelRatioChange)) {
        raise();
        updateGeometry();
        update();
        scheduleRefresh();
    }
    return QWidget::eventFilter(watched, event);
}

void PlaybackIndicatorOverlay::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    if (!m_indicatorVisible || !isPositionVisible(m_position))
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(m_color);

    if (m_shape == Shape::Line) {
        pen.setWidthF(1.0);
        painter.setPen(pen);
        painter.drawLine(QLineF(m_position, 0, m_position, height()));
        return;
    }

    constexpr qreal penWidth = 2.0;
    constexpr qreal triangleWidth = 12.0;
    constexpr qreal triangleHeight = 1.73205 * triangleWidth / 2.0;
    pen.setWidthF(penWidth);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(m_color);
    const auto top = height() - triangleHeight - penWidth;
    const QPointF points[] = {
        {m_position - triangleWidth / 2.0, top                 },
        {m_position + triangleWidth / 2.0, top                 },
        {m_position,                       top + triangleHeight}
    };
    painter.drawPolygon(points, 3);
}

QRect PlaybackIndicatorOverlay::indicatorRect(const qreal position) const {
    if (!isPositionVisible(position))
        return {};
    if (m_shape == Shape::Line)
        return QRectF(position - 2.0, 0, 4.0, height()).toAlignedRect();

    constexpr qreal triangleWidth = 12.0;
    constexpr qreal triangleHeight = 1.73205 * triangleWidth / 2.0;
    return QRectF(position - triangleWidth / 2.0 - 2.0, height() - triangleHeight - 4.0,
                  triangleWidth + 4.0, triangleHeight + 4.0)
        .toAlignedRect();
}

bool PlaybackIndicatorOverlay::isPositionVisible(const qreal position) const {
    if (!std::isfinite(position))
        return false;
    const qreal margin = m_shape == Shape::Line ? 1.0 : 8.0;
    return position >= -margin && position < width() + margin;
}

void PlaybackIndicatorOverlay::scheduleRefresh() {
    if (m_refreshPending)
        return;
    m_refreshPending = true;
    QMetaObject::invokeMethod(
        this,
        [this] {
            m_refreshPending = false;
            updateGeometry();
            raise();
            update();
        },
        Qt::QueuedConnection);
}

void PlaybackIndicatorOverlay::updateGeometry() {
    if (const auto parent = parentWidget())
        setGeometry(parent->rect());
}
