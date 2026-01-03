//
// Created by fluty on 2024/2/3.
//

#include "TimelineView.h"

#include <QPainter>
#include <QWheelEvent>

#include "Controller/PlaybackController.h"
#include "Model/AppModel/LoopSettings.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppStatus/AppStatus.h"
#include "Model/AppModel/InferPiece.h"
#include "Model/AppModel/Note.h"
#include "Utils/MathUtils.h"

TimelineView::TimelineView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);
    setObjectName("TimelineView");
    setMouseTracking(true);

    connect(this, &TimelineView::setLastPositionTriggered, playbackController, [=](double tick) {
        playbackController->setLastPosition(tick);
        playbackController->setPosition(tick);
    });
    connect(playbackController, &PlaybackController::positionChanged, this,
            &TimelineView::setPosition);
    connect(appModel, &AppModel::modelChanged, this, [this] {
        setTimeSignature(appModel->timeSignature().numerator,
                         appModel->timeSignature().denominator);
    });
    connect(appModel, &AppModel::timeSignatureChanged, this, &TimelineView::setTimeSignature);
    connect(appStatus, &AppStatus::quantizeChanged, this, &TimelineView::setQuantize);
    connect(appStatus, &AppStatus::loopSettingsChanged, this, &TimelineView::onLoopSettingsChanged);
}

void TimelineView::setCanEditLoop(bool canEdit) {
    m_canEditLoop = canEdit;
}

void TimelineView::setTimeRange(double startTick, double endTick) {
    m_startTick = startTick;
    m_endTick = endTick;
    update();
}

void TimelineView::setTimeSignature(int numerator, int denominator) {
    ITimelinePainter::setTimeSignature(numerator, denominator);
    update();
}

void TimelineView::setPosition(double tick) {
    m_position = tick;
    update();
}

void TimelineView::setQuantize(int quantize) {
    ITimelinePainter::setQuantize(quantize);
    update();
}

void TimelineView::setDataContext(SingingClip *clip) {
    if (!clip) {
        if (m_clip)
            disconnect(m_clip, nullptr, this, nullptr);
        onPiecesChanged({});
    } else {
        onPiecesChanged(clip->pieces());
        connect(clip, &SingingClip::piecesChanged, this, &TimelineView::onPiecesChanged);
        m_pieces = clip->pieces();
    }
    m_clip = clip;
    update();
}

// void TimelineView::setPieces(const QList<InferPiece *> &pieces) {
//     m_pieces = pieces;
//     update();
// }

void TimelineView::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw background
    // painter.setPen(Qt::NoPen);
    // painter.setBrush(QColor(28, 29, 30));
    // painter.drawRect(rect());
    // painter.setBrush(Qt::NoBrush);

    // Draw graduates
    drawTimeline(&painter, m_startTick, m_endTick, rect().width());

    // Draw Pieces
    if (m_clip)
        drawPieces(&painter);

    // Draw loop region
    drawLoopRegion(&painter);

    // Draw playback indicator
    auto penWidth = 2.0;
    auto color = QColor(200, 200, 200);
    QPen pen;
    pen.setWidthF(penWidth);
    pen.setColor(color);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(color);

    auto centerX = tickToX(m_position);
    double w = 12;
    double h = 1.73205 * w / 2;
    auto marginTop = rect().height() - h - penWidth;
    auto p1 = QPointF(centerX - w / 2, marginTop);
    auto p2 = QPointF(centerX + w / 2, marginTop);
    auto p3 = QPointF(centerX, marginTop + h);
    QPointF points[3]{p1, p2, p3};
    painter.drawPolygon(points, 3);
}

void TimelineView::drawBar(QPainter *painter, int tick, int bar) {
    QPen pen;
    auto x = tickToX(tick); // tick to itemX
    pen.setColor(QColor(200, 200, 200));
    painter->setPen(pen);
    auto text = bar > 0 ? QString::number(bar) : QString::number(bar - 1);

    if (!m_textCache.contains("Bar") || !m_textCache["Bar"].contains(text) ||
        m_textCache["Bar"][text].isNull())
        cacheText("Bar", text, *painter);
    const auto &pixmap = m_textCache["Bar"][text];
    const QRectF textRect(x + m_textPaddingLeft, m_loopRegionHeight, pixmap.width(), pixmap.height());
    painter->drawPixmap(textRect.topLeft(), pixmap);
    // painter->drawText(QPointF(x + m_textPaddingLeft, 10), text);
    pen.setColor(QColor(92, 96, 100));
    painter->setPen(pen);
    auto y1 = rect().height() - 24;
    auto y2 = rect().height();
    painter->drawLine(QLineF(x, y1, x, y2));
}

void TimelineView::drawBeat(QPainter *painter, int tick, int bar, int beat) {
    QPen pen;
    auto x = tickToX(tick);
    pen.setColor(QColor(160, 160, 160));
    painter->setPen(pen);
    // 在负坐标获取的 int bar 错误，暂不绘制文本
    if (beat > 0) {
        const auto text = QString::number(beat);
        if (!m_textCache.contains("Beat") || !m_textCache["Beat"].contains(text) ||
            m_textCache["Beat"][text].isNull())
            cacheText("Beat", text, *painter);
        const auto &pixmap = m_textCache["Beat"][text];
        const QRectF textRect(x + m_textPaddingLeft, m_loopRegionHeight, pixmap.width(), pixmap.height());
        painter->drawPixmap(textRect.topLeft(), pixmap);
        // painter->drawText(QPointF(x + m_textPaddingLeft, 10),
        //           /*QString::number(bar) + "." +*/ QString::number(beat));
    }

    pen.setColor(QColor(72, 75, 78));
    painter->setPen(pen);
    auto y1 = rect().height() - 16;
    auto y2 = rect().height();
    painter->drawLine(QLineF(x, y1, x, y2));
}

void TimelineView::drawEighth(QPainter *painter, int tick) {
    QPen pen;
    auto x = tickToX(tick);
    pen.setColor(QColor(57, 59, 61));
    painter->setPen(pen);
    auto y1 = rect().height() - 8;
    auto y2 = rect().height();
    painter->drawLine(QLineF(x, y1, x, y2));
}

void TimelineView::wheelEvent(QWheelEvent *event) {
    emit wheelHorScale(event);
    QWidget::wheelEvent(event);
}

void TimelineView::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        const auto loopSettings = appStatus->loopSettings.get();
        if (loopSettings.enabled && m_canEditLoop) {
            m_loopDragMode = hitTestLoop(event->pos());
            if (m_loopDragMode != None) {
                m_loopDragStartTick = loopSettings.start;
                m_loopDragStartPos = static_cast<int>(xToTick(event->pos().x()));
                event->accept();
                return;
            }
        }
        emit setLastPositionTriggered(xToTick(event->position().x()));
    }
    event->ignore();
}

void TimelineView::mouseMoveEvent(QMouseEvent *event) {
    if (m_loopDragMode != None && m_canEditLoop) {
        auto loopSettings = appStatus->loopSettings.get();
        const bool noSnap = event->modifiers() & Qt::AltModifier;
        const int quantizeInterval = noSnap ? 1 : 1920 / appStatus->quantize;
        const int currentTick = MathUtils::round(static_cast<int>(xToTick(event->pos().x())), quantizeInterval);
        const int deltaTick = currentTick - m_loopDragStartPos;

        if (m_loopDragMode == DragStart) {
            int newStart = qMax(0, MathUtils::round(m_loopDragStartTick + deltaTick, quantizeInterval));
            int newLength = loopSettings.length - (newStart - loopSettings.start);
            if (newLength >= quantizeInterval) {
                loopSettings.start = newStart;
                loopSettings.length = newLength;
            }
        } else if (m_loopDragMode == DragEnd) {
            int newLength = qMax(quantizeInterval, loopSettings.length + deltaTick);
            loopSettings.length = newLength;
            m_loopDragStartPos = currentTick;
        } else if (m_loopDragMode == DragBody) {
            int newStart = qMax(0, MathUtils::round(m_loopDragStartTick + deltaTick, quantizeInterval));
            loopSettings.start = newStart;
        }
        appStatus->loopSettings.set(loopSettings);
        event->accept();
        return;
    }

    updateCursor(event->pos());

    if (event->buttons() & Qt::LeftButton) {
        emit setLastPositionTriggered(xToTick(event->position().x()));
    }
    QWidget::mouseMoveEvent(event);
}

void TimelineView::mouseReleaseEvent(QMouseEvent *event) {
    if (m_loopDragMode != None) {
        m_loopDragMode = None;
        updateCursor(event->pos());
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void TimelineView::leaveEvent(QEvent *event) {
    setCursor(Qt::ArrowCursor);
    QWidget::leaveEvent(event);
}

void TimelineView::onPiecesChanged(const QList<InferPiece *> &pieces) {
    for (const auto piece : m_pieces) {
        disconnect(piece, nullptr, this, nullptr);
    }
    for (const auto piece : pieces) {
        connect(piece, &InferPiece::statusChanged, this, [this] { update(); });
    }
    m_pieces = pieces;
    update();
}

void TimelineView::onLoopSettingsChanged(const LoopSettings &settings) {
    Q_UNUSED(settings)
    update();
}

void TimelineView::drawPieces(QPainter *painter) const {
    auto penWidth = 2;
    auto y = rect().height() - penWidth / 2;
    QPen pen;
    pen.setWidthF(penWidth);
    pen.setCapStyle(Qt::RoundCap);
    painter->setBrush(Qt::NoBrush);
    for (const auto &piece : m_clip->pieces()) {
        pen.setColor(m_piecesColors[piece->acousticInferStatus]);
        painter->setPen(pen);
        auto pieceStartX = tickToX(piece->localStartTick() + m_clip->start());
        auto pieceEndX = tickToX(piece->localEndTick() + m_clip->start());
        painter->drawLine(pieceStartX, y, pieceEndX, y);
        painter->drawText(QPointF(pieceStartX, y), "#" + QString::number(piece->id()));

        // For debugging
        auto firstNoteStartTick = piece->notes.first()->localStart() + m_clip->start();
        auto firstNoteStartX = tickToX(firstNoteStartTick);

        // TODO: handle multiple tempo changes
        auto headX = tickToX(firstNoteStartTick - appModel->msToTick(piece->headAvailableLengthMs));

        auto lastNoteEndTick =
            piece->notes.last()->localStart() + piece->notes.last()->length() + m_clip->start();
        auto lastNoteEndX = tickToX(lastNoteEndTick);

        // Draw head available range
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(92, 206, 219, 64));
        painter->drawRect(QRectF(headX, 0, pieceStartX - headX, rect().height()));

        painter->setPen(QColor(92, 206, 219));
        painter->drawLine(headX, 0, headX, rect().height());

        // Draw padding start range
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(222, 164, 224, 64));
        painter->drawRect(QRectF(pieceStartX, 0, firstNoteStartX - pieceStartX, rect().height()));

        painter->setPen(QColor(222, 164, 224));
        painter->drawLine(pieceStartX, 0, pieceStartX, rect().height());

        // Draw notes range
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(155, 186, 255, 64));
        painter->drawRect(
            QRectF(firstNoteStartX, 0, lastNoteEndX - firstNoteStartX, rect().height()));

        painter->setPen(QColor(155, 186, 255));
        painter->drawLine(firstNoteStartX, 0, firstNoteStartX, rect().height());
        painter->drawLine(lastNoteEndX, 0, lastNoteEndX, rect().height());

        // Draw padding end range
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(184, 196, 116, 64));
        painter->drawRect(QRectF(lastNoteEndX, 0, pieceEndX - lastNoteEndX, rect().height()));

        painter->setPen(QColor(184, 196, 116));
        painter->drawLine(pieceEndX, 0, pieceEndX, rect().height());
    }
}

double TimelineView::tickToX(double tick) const {
    auto ratio = (tick - m_startTick) / (m_endTick - m_startTick);
    auto x = rect().width() * ratio;
    return x;
}

double TimelineView::xToTick(double x) const {
    auto tick = 1.0 * x / rect().width() * (m_endTick - m_startTick) + m_startTick;
    if (tick < 0)
        tick = 0;
    return tick;
}

void TimelineView::cacheText(const QString &type, const QString &text, const QPainter &painter) {
    // qDebug() << "cacheText:" << text;
    const QSize textSize = painter.fontMetrics().size(Qt::TextSingleLine, text);
    QPixmap pixmap(textSize * painter.device()->devicePixelRatio());
    pixmap.setDevicePixelRatio(painter.device()->devicePixelRatio());
    pixmap.fill(Qt::transparent);

    QPainter cachePainter(&pixmap);
    cachePainter.setPen(painter.pen());
    cachePainter.drawText(pixmap.rect(), text);

    m_textCache[type].insert(text, pixmap);
}

void TimelineView::drawLoopRegion(QPainter *painter) const {
    const auto loopSettings = appStatus->loopSettings.get();
    // Show loop region if there's data, even when disabled
    if (loopSettings.length <= 0)
        return;

    const double startX = tickToX(loopSettings.start);
    const double endX = tickToX(loopSettings.end());
    const int triangleSize = m_loopRegionHeight;

    // Loop region color - gray when disabled, blue when enabled
    const QColor loopColor = loopSettings.enabled ? QColor(155, 186, 255) : QColor(57, 59, 61);

    painter->setPen(Qt::NoPen);
    painter->setBrush(loopColor);

    // Draw connecting line at top (y=0), overlapping with triangles
    const double lineHeight = 4;
    painter->drawRect(QRectF(startX, 0, endX - startX, lineHeight));

    // Draw left triangle (right-angle triangle pointing right)
    QPolygonF leftTriangle;
    leftTriangle << QPointF(startX, 0)
                 << QPointF(startX + triangleSize, 0)
                 << QPointF(startX, triangleSize);
    painter->drawPolygon(leftTriangle);

    // Draw right triangle (right-angle triangle pointing left)
    QPolygonF rightTriangle;
    rightTriangle << QPointF(endX - triangleSize, 0)
                  << QPointF(endX, 0)
                  << QPointF(endX, triangleSize);
    painter->drawPolygon(rightTriangle);
}

TimelineView::LoopDragMode TimelineView::hitTestLoop(const QPoint &pos) const {
    const auto loopSettings = appStatus->loopSettings.get();
    if (!loopSettings.enabled)
        return None;

    const double startX = tickToX(loopSettings.start);
    const double endX = tickToX(loopSettings.end());

    // Check if in loop region vertical area
    if (pos.y() > m_loopRegionHeight + 4)
        return None;

    // Check start handle
    if (pos.x() >= startX - 2 && pos.x() <= startX + m_loopHandleWidth + 2)
        return DragStart;

    // Check end handle
    if (pos.x() >= endX - m_loopHandleWidth - 2 && pos.x() <= endX + 2)
        return DragEnd;

    // Check body
    if (pos.x() > startX + m_loopHandleWidth && pos.x() < endX - m_loopHandleWidth)
        return DragBody;

    return None;
}

void TimelineView::updateCursor(const QPoint &pos) {
    if (!m_canEditLoop) {
        setCursor(Qt::ArrowCursor);
        return;
    }

    const auto mode = hitTestLoop(pos);
    switch (mode) {
        case DragStart:
        case DragEnd:
            setCursor(Qt::SizeHorCursor);
            break;
        case DragBody:
            setCursor(Qt::OpenHandCursor);
            break;
        default:
            setCursor(Qt::ArrowCursor);
            break;
    }
}