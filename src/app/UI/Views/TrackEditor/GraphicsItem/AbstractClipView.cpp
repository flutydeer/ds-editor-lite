//
// Created by fluty on 2023/11/14.
//
#include "AbstractClipView.h"
#include "AbstractClipGraphicsItem_p.h"

#include "Global/AppGlobal.h"
#include "Global/TracksEditorGlobal.h"
#include "UI/Controls/Menu.h"

#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPainter>

using namespace TracksEditorGlobal;

AbstractClipView::AbstractClipView(int itemId, QGraphicsItem *parent)
    : CommonGraphicsRectItem(parent), IClip(itemId),
      d_ptr(new AbstractClipViewPrivate(this)) {
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

    auto font = QFont();
    font.setPointSizeF(10);
    painter->setFont(font);
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
    auto controlStr = (d->m_showDebugInfo ? QString("id: %1 ").arg(id()) : "") +
                      QString("%1 %2dB %3 ")
                          .arg(d->m_name)
                          .arg(QString::number(d->m_gain))
                          .arg(d->m_mute ? "M" : "");
    auto timeStr = QString("s: %1 l: %2 cs: %3 cl: %4 sx: %5 sy: %6")
                       .arg(d->m_start)
                       .arg(d->m_length)
                       .arg(d->m_clipStart)
                       .arg(d->m_clipLen)
                       .arg(scaleX())
                       .arg(scaleY());
    auto text = clipTypeName() + controlStr + (d->m_showDebugInfo ? timeStr : "");
    auto textWidth = fontMetrics.horizontalAdvance(text);

    pen.setColor(colorForeground);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    // painter->drawRect(textRect);
    if (textWidth <= textRectWidth && textHeight <= textRectHeight) {
        if (d->previewRect().height() < 32)
            painter->drawText(textRect, text, QTextOption(Qt::AlignVCenter));
        else
            painter->drawText(textRect, text);
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

/*void AbstractClipView::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    Q_D(AbstractClipView);
    if (event->button() != Qt::LeftButton) {
        d->m_mouseMoveBehavior = AbstractClipGraphicsItemPrivate::None;
        setCursor(Qt::ArrowCursor);
    }

    // setSelected(true);
    d->m_mouseDownPos = event->scenePos();
    d->m_mouseDownStart = start();
    d->m_mouseDownClipStart = clipStart();
    d->m_mouseDownLength = length();
    d->m_mouseDownClipLen = clipLen();
    event->accept(); // Must accept event, or mouseMoveEvent would not work.
}
void AbstractClipView::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    Q_D(AbstractClipView);
    auto curPos = event->scenePos();
    if (event->modifiers() == Qt::AltModifier)
        d->m_tempQuantizeOff = true;
    else
        d->m_tempQuantizeOff = false;

    auto dx = (curPos.x() - d->m_mouseDownPos.x()) / scaleX() / pixelsPerQuarterNote * 480;

    int start;
    int clipStart;
    int left;
    int clipLen;
    int right;
    int delta = qRound(dx);
    int quantize = d->m_tempQuantizeOff ? 1 : 1920 / d->m_quantize;
    d->m_propertyEdited = true;
    switch (d->m_mouseMoveBehavior) {
        case AbstractClipGraphicsItemPrivate::Move:
            left =
                MathUtils::round(d->m_mouseDownStart + d->m_mouseDownClipStart + delta, quantize);
            start = left - d->m_mouseDownClipStart;
            setStart(start);
            break;
        case AbstractClipGraphicsItemPrivate::ResizeLeft:
            left =
                MathUtils::round(d->m_mouseDownStart + d->m_mouseDownClipStart + delta, quantize);
            start = d->m_mouseDownStart;
            clipStart = left - start;
            clipLen = d->m_mouseDownStart + d->m_mouseDownClipStart + d->m_mouseDownClipLen - left;
            if (clipLen <= 0)
                break;

            if (clipStart < 0) {
                setClipStart(0);
                setClipLen(d->m_mouseDownClipStart + d->m_mouseDownClipLen);
            } else if (clipStart <= d->m_mouseDownClipStart + d->m_mouseDownClipLen) {
                setClipStart(clipStart);
                setClipLen(clipLen);
            } else {
                setClipStart(d->m_mouseDownClipStart + d->m_mouseDownClipLen);
                setClipLen(0);
            }
            break;
        case AbstractClipGraphicsItemPrivate::ResizeRight:
            right = MathUtils::round(d->m_mouseDownStart + d->m_mouseDownClipStart +
                                         d->m_mouseDownClipLen + delta,
                                     quantize);
            clipLen = right - (d->m_mouseDownStart + d->m_mouseDownClipStart);
            if (clipLen <= 0)
                break;

            if (!d->m_canResizeLength) {
                if (d->m_clipStart + clipLen >= d->m_length)
                    setClipLen(d->m_length - d->m_clipStart);
                else
                    setClipLen(clipLen);
            } else {
                setLength(d->m_clipStart + clipLen);
                setClipLen(clipLen);
            }
            break;
        case AbstractClipGraphicsItemPrivate::None:
            d->m_propertyEdited = false;
            break;
    }
    QGraphicsRectItem::mouseMoveEvent(event);
}
void AbstractClipView::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    Q_D(AbstractClipView);
    if (d->m_mouseDownPos == event->scenePos()) {
        d->m_propertyEdited = false;
    }
    if (d->m_propertyEdited) {
        emit propertyChanged();
    }

    CommonGraphicsRectItem::mouseReleaseEvent(event);
}*/
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