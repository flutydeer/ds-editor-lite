//
// Created by fluty on 2023/11/14.
//
#include "AbstractClipView.h"
#include "AbstractClipView_p.h"

#include "Global/AppGlobal.h"
#include "Global/TracksEditorGlobal.h"
#include "UI/Controls/Menu.h"

#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QSvgRenderer>

using namespace TracksEditorGlobal;

AbstractClipView::AbstractClipView(const int itemId, QGraphicsItem *parent)
    : AbstractGraphicsRectItem(parent), IClip(itemId), d_ptr(new AbstractClipViewPrivate(this)) {
    setAcceptHoverEvents(true);
    setFlag(ItemIsSelectable);
}

AbstractClipView::~AbstractClipView() {
    disconnect();
    delete d_ptr;
    // qDebug() << "~AbstractClipView()";
}

QString AbstractClipView::name() const {
    Q_D(const AbstractClipView);
    return d->m_name;
}

void AbstractClipView::setName(const QString &text) {
    Q_D(AbstractClipView);
    d->m_name = text;
    update();
}

int AbstractClipView::start() const {
    Q_D(const AbstractClipView);
    return d->m_start;
}

void AbstractClipView::setStart(const int start) {
    Q_D(AbstractClipView);
    d->m_start = start;
    updateRectAndPos();
}

int AbstractClipView::length() const {
    Q_D(const AbstractClipView);
    return d->m_length;
}

void AbstractClipView::setLength(const int length) {
    Q_D(AbstractClipView);
    d->m_length = length;
    updateRectAndPos();
}

int AbstractClipView::clipStart() const {
    Q_D(const AbstractClipView);
    return d->m_clipStart;
}

void AbstractClipView::setClipStart(const int clipStart) {
    Q_D(AbstractClipView);
    d->m_clipStart = clipStart;
    updateRectAndPos();
}

int AbstractClipView::clipLen() const {
    Q_D(const AbstractClipView);
    return d->m_clipLen;
}

void AbstractClipView::setClipLen(const int clipLen) {
    Q_D(AbstractClipView);
    d->m_clipLen = clipLen;
    updateRectAndPos();
}

double AbstractClipView::gain() const {
    Q_D(const AbstractClipView);
    return d->m_gain;
}

void AbstractClipView::setGain(const double gain) {
    Q_D(AbstractClipView);
    d->m_gain = gain;
    update();
}

bool AbstractClipView::mute() const {
    Q_D(const AbstractClipView);
    return d->m_mute;
}

void AbstractClipView::setMute(const bool mute) {
    Q_D(AbstractClipView);
    d->m_mute = mute;
    update();
}

bool AbstractClipView::activeClip() const {
    Q_D(const AbstractClipView);
    return d->m_activeClip;
}

void AbstractClipView::setActiveClip(const bool active) {
    Q_D(AbstractClipView);
    d->m_activeClip = active;
    update();
}

int AbstractClipView::trackIndex() const {
    Q_D(const AbstractClipView);
    return d->m_trackIndex;
}

void AbstractClipView::setTrackIndex(const int index) {
    Q_D(AbstractClipView);
    d->m_trackIndex = index;
    updateRectAndPos();
}

bool AbstractClipView::canResizeLength() const {
    Q_D(const AbstractClipView);
    return d->m_canResizeLength;
}

int AbstractClipView::contentLength() const {
    return 0;
}

void AbstractClipView::loadCommonProperties(const Clip::ClipCommonProperties &args) {
    Q_D(AbstractClipView);
    d->m_name = args.name;
    d->m_start = args.start;
    d->m_length = args.length;
    d->m_clipStart = args.clipStart;
    d->m_clipLen = args.clipLen;
    d->m_gain = args.gain;
    d->m_mute = args.mute;
    updateRectAndPos();
}

void AbstractClipView::setQuantize(const int quantize) {
    Q_D(AbstractClipView);
    d->m_quantize = quantize;
}

QRectF AbstractClipViewPrivate::previewRect() const {
    Q_Q(const AbstractClipView);
    constexpr auto penWidth = 1.2f;
    constexpr auto verticalPadding = 1.2f;
    const auto rect = q->rect();
    const auto left = rect.left() + penWidth / 2;
    const auto top = rect.top() + titleHeight + verticalPadding;
    const auto width = rect.width() - penWidth;
    const auto height = rect.height() - titleHeight - verticalPadding * 2;
    const auto paddedRect = QRectF(left, top, width, height);
    return paddedRect;
}

QString AbstractClipView::text() const {
    Q_D(const AbstractClipView);
    const auto controlStr =
        (d->m_showDebugInfo ? QString("id: %1 ").arg(id()) : "") +
        QString("%1 %2dB %3 ").arg(name()).arg(QString::number(gain())).arg(mute() ? "M" : "");
    const auto timeStr = QString("s: %1 l: %2 cs: %3 cl: %4 sx: %5 sy: %6")
                             .arg(start())
                             .arg(length())
                             .arg(clipStart())
                             .arg(clipLen())
                             .arg(scaleX())
                             .arg(scaleY());
    return controlStr + (d->m_showDebugInfo ? timeStr : "");
}

void AbstractClipView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                             QWidget *widget) {
    Q_D(const AbstractClipView);
    constexpr auto colorPrimary = QColor(155, 186, 255);
    constexpr auto colorPrimaryTransparent = QColor(155, 186, 255, 64);
    constexpr auto colorPrimaryDarker = QColor(112, 156, 255);
    constexpr auto colorPrimaryLighter = QColor(205, 221, 255);
    constexpr auto colorForeground = QColor(0, 0, 0);
    auto penWidth = 1.2f;
    auto verticalPadding = 1.2f;

    auto left = rect().left() + penWidth / 2;
    auto top = rect().top() + verticalPadding;
    auto width = rect().width() - penWidth;
    auto height = rect().height() - verticalPadding * 2;
    auto paddedRect = QRectF(left, top, width, height);
    auto radius = 4;

    // double iconWidth = 16;
    double iconWidth = 4;
    double textPadding = 0;
    auto rectLeft = mapToScene(rect().topLeft()).x();
    auto rectRight = mapToScene(rect().bottomRight()).x();
    auto titleRectLeft = visibleRect().left() < rectLeft
                             ? paddedRect.left() + textPadding
                             : visibleRect().left() - rectLeft + textPadding + penWidth / 2;
    auto titleRectTop = paddedRect.top() + textPadding;
    auto titleRectWidth = rectLeft < visibleRect().left()
                              ? rectRight - visibleRect().left() - 2 * (textPadding + penWidth)
                              : rectRight - rectLeft - 2 * (textPadding + penWidth);
    auto titleRectHeight = paddedRect.height() - 2 * textPadding;

    auto previewRectHeight = d->previewRect().height();

    // Draw Background
    QPen pen;
    pen.setWidthF(penWidth);
    if (previewRectHeight >= 32) {
        painter->setBrush(colorPrimaryTransparent);
    } else
        painter->setBrush(isSelected() ? colorPrimaryLighter : colorPrimary);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(paddedRect, radius, radius);

    QColor borderColor;
    if (isSelected()) {
        borderColor = {255, 255, 255};
    } else if (activeClip()) {
        borderColor = colorPrimary;
    }

    // Draw title
    if (previewRectHeight >= 32) {
        QPainterPath titleBackgroundPath;
        titleBackgroundPath.moveTo(paddedRect.left(), d->titleHeight);
        titleBackgroundPath.lineTo(paddedRect.right(), d->titleHeight);
        titleBackgroundPath.lineTo(paddedRect.right(), paddedRect.top() + radius);
        titleBackgroundPath.arcTo(paddedRect.right() - 2 * radius, paddedRect.top(), 2 * radius,
                                  2 * radius, 0, 90);
        titleBackgroundPath.lineTo(paddedRect.left() + radius, paddedRect.top());
        titleBackgroundPath.arcTo(paddedRect.left(), paddedRect.top(), 2 * radius, 2 * radius, 90,
                                  90);
        titleBackgroundPath.lineTo(paddedRect.left(), d->titleHeight);
        painter->setPen(Qt::NoPen);
        painter->setBrush(isSelected() ? colorPrimaryLighter : colorPrimary);
        // painter->setBrush(colorPrimary);
        painter->drawPath(titleBackgroundPath);
    }

    // Draw Border
    if (isSelected() || activeClip()) {
        pen.setColor(borderColor);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(paddedRect, radius, radius);
    }

    auto textRect = QRectF(titleRectLeft + iconWidth, titleRectTop, titleRectWidth - iconWidth,
                           titleRectHeight);

    auto fontMetrics = painter->fontMetrics();
    auto textHeight = fontMetrics.height();
    auto textWidth = fontMetrics.horizontalAdvance(text());

    // Draw text and icon
    pen.setColor(colorForeground);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    // QRectF iconRect;
    if (textWidth + iconWidth <= titleRectWidth && textHeight <= d->titleHeight) {
        if (d->previewRect().height() < 32) {
            painter->drawText(textRect, text(), QTextOption(Qt::AlignVCenter));
            // iconRect = QRectF(titleRectLeft, titleRectTop, iconWidth, titleRectHeight);
        } else {
            painter->drawText(textRect, text());
            // iconRect = QRectF(titleRectLeft, titleRectTop, iconWidth, textHeight);
        }
        // QSvgRenderer renderer(iconPath());
        // renderer.setAspectRatioMode(Qt::KeepAspectRatio);
        // renderer.render(painter, iconRect);
    }

    if (previewRectHeight >= 32) {
        // auto colorAlpha = previewRectHeight <= 48
        //                       ? static_cast<int>(255 * (previewRectHeight - 32) / (48 - 32))
        //                       : 255;
        painter->setClipRect(d->previewRect());
        drawPreviewArea(painter, d->previewRect(),
                        isSelected() ? colorPrimaryLighter : colorPrimary);
    }
}

void AbstractClipView::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    const auto rx = event->pos().rx();
    if (rx >= 0 && rx <= AppGlobal::resizeTolerance ||
        rx >= rect().width() - AppGlobal::resizeTolerance && rx <= rect().width())
        setCursor(Qt::SizeHorCursor);
    else
        setCursor(Qt::ArrowCursor);

    QGraphicsRectItem::hoverMoveEvent(event);
}

void AbstractClipView::updateRectAndPos() {
    Q_D(AbstractClipView);
    const auto x = (d->m_start + d->m_clipStart) * scaleX() * pixelsPerQuarterNote / 480;
    const auto y = d->m_trackIndex * trackHeight * scaleY();
    const auto w = d->m_clipLen * scaleX() * pixelsPerQuarterNote / 480;
    const auto h = trackHeight * scaleY();
    setPos(x, y);
    setRect(QRectF(0, 0, w, h));
    update();
}

void AbstractClipView::setCanResizeLength(const bool on) {
    Q_D(AbstractClipView);
    d->m_canResizeLength = on;
}

double AbstractClipView::tickToSceneX(const double tick) const {
    return tick * scaleX() * pixelsPerQuarterNote / 480;
}

double AbstractClipView::sceneXToItemX(const double x) const {
    return mapFromScene(QPointF(x, 0)).x();
}