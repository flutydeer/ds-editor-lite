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

using namespace TracksEditorGlobal;

AbstractClipView::AbstractClipView(int itemId, QGraphicsItem *parent)
    : CommonGraphicsRectItem(parent), IClip(itemId), d_ptr(new AbstractClipViewPrivate(this)) {
    setAcceptHoverEvents(true);
    setFlag(ItemIsSelectable);
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
void AbstractClipView::setMute(bool mute) {
    Q_D(AbstractClipView);
    d->m_mute = mute;
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
void AbstractClipView::setQuantize(int quantize) {
    Q_D(AbstractClipView);
    d->m_quantize = quantize;
}
QRectF AbstractClipViewPrivate::previewRect() const {
    Q_Q(const AbstractClipView);
    auto penWidth = 2.0f;
    auto paddingTop = 20;
    auto rect = q->boundingRect();
    auto left = rect.left() + penWidth;
    auto top = rect.top() + paddingTop + penWidth;
    auto width = rect.width() - penWidth * 2;
    auto height = rect.height() - paddingTop - penWidth * 2;
    auto paddedRect = QRectF(left, top, width, height);
    return paddedRect;
}

QString AbstractClipView::text() const {
    Q_D(const AbstractClipView);
    auto controlStr =
        (d->m_showDebugInfo ? QString("id: %1 ").arg(id()) : "") +
        QString("%1 %2dB %3 ").arg(name()).arg(QString::number(gain())).arg(mute() ? "M" : "");
    auto timeStr = QString("s: %1 l: %2 cs: %3 cl: %4 sx: %5 sy: %6")
                       .arg(start())
                       .arg(length())
                       .arg(clipStart())
                       .arg(clipLen())
                       .arg(scaleX())
                       .arg(scaleY());
    return clipTypeName() + controlStr + (d->m_showDebugInfo ? timeStr : "");
}
void AbstractClipView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                             QWidget *widget) {
    Q_D(const AbstractClipView);
    const auto colorPrimary = QColor(155, 186, 255);
    const auto colorPrimaryDarker = QColor(112, 156, 255);
    const auto colorForeground = QColor(0, 0, 0);
    auto penWidth = 2.0f;

    QPen pen;
    if (isSelected())
        pen.setColor(QColor(255, 255, 255));
    else if (overlapped())
        pen.setColor(AppGlobal::overlappedViewBorder);
    else
        pen.setColor(colorPrimaryDarker);

    auto rect = boundingRect();
    auto left = rect.left() + penWidth;
    auto top = rect.top() + penWidth;
    auto width = rect.width() - penWidth * 2;
    auto height = rect.height() - penWidth * 2;
    auto paddedRect = QRectF(left, top, width, height);

    pen.setWidthF(penWidth);
    painter->setPen(pen);
    painter->setBrush(colorPrimary);

    painter->drawRoundedRect(paddedRect, 4, 4);

    double textPadding = 2;
    auto rectLeft = mapToScene(rect.topLeft()).x();
    auto rectRight = mapToScene(rect.bottomRight()).x();
    auto textRectLeft = visibleRect().left() < rectLeft
                            ? paddedRect.left() + textPadding
                            : visibleRect().left() - rectLeft + textPadding + penWidth / 2;
    auto textRectTop = paddedRect.top() + textPadding;
    auto textRectWidth = rectLeft < visibleRect().left()
                             ? rectRight - visibleRect().left() - 2 * (textPadding + penWidth)
                             : rectRight - rectLeft - 2 * (textPadding + penWidth);
    auto textRectHeight = paddedRect.height() - 2 * textPadding;
    auto textRect = QRectF(textRectLeft, textRectTop, textRectWidth, textRectHeight);

    auto fontMetrics = painter->fontMetrics();
    auto textHeight = fontMetrics.height();
    auto textWidth = fontMetrics.horizontalAdvance(text());

    pen.setColor(colorForeground);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    // painter->drawRect(textRect);
    if (textWidth <= textRectWidth && textHeight <= textRectHeight) {
        if (d->previewRect().height() < 32)
            painter->drawText(textRect, text(), QTextOption(Qt::AlignVCenter));
        else
            painter->drawText(textRect, text());
    }
    // pen.setColor(Qt::red);
    // painter->setPen(pen);
    // painter->drawRect(textRect);

    auto previewRectHeight = d->previewRect().height();
    if (previewRectHeight >= 32) {
        auto colorAlpha = previewRectHeight <= 48
                              ? static_cast<int>(255 * (previewRectHeight - 32) / (48 - 32))
                              : 255;
        drawPreviewArea(painter, d->previewRect(), colorAlpha);
    }
}
void AbstractClipView::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    const auto rx = event->pos().rx();
    if (rx >= 0 && rx <= AppGlobal::resizeTolarance ||
        rx >= rect().width() - AppGlobal::resizeTolarance && rx <= rect().width())
        setCursor(Qt::SizeHorCursor);
    else
        setCursor(Qt::ArrowCursor);

    QGraphicsRectItem::hoverMoveEvent(event);
}

void AbstractClipView::updateRectAndPos() {
    Q_D(AbstractClipView);
    auto x = (d->m_start + d->m_clipStart) * scaleX() * pixelsPerQuarterNote / 480;
    auto y = d->m_trackIndex * trackHeight * scaleY();
    auto w = d->m_clipLen * scaleX() * pixelsPerQuarterNote / 480;
    auto h = trackHeight * scaleY();
    setPos(x, y);
    setRect(QRectF(0, 0, w, h));
    update();
}
void AbstractClipView::setCanResizeLength(bool on) {
    Q_D(AbstractClipView);
    d->m_canResizeLength = on;
}
double AbstractClipView::tickToSceneX(const double tick) const {
    return tick * scaleX() * pixelsPerQuarterNote / 480;
}
double AbstractClipView::sceneXToItemX(const double x) const {
    return mapFromScene(QPointF(x, 0)).x();
}