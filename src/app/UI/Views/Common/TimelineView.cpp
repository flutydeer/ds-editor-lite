//
// Created by fluty on 2024/2/3.
//

#include "TimelineView.h"

#include <QPainter>
#include <QWheelEvent>

#include "Controller/PlaybackController.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppStatus/AppStatus.h"
#include "Model/Inference/InferPiece.h"

TimelineView::TimelineView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);
    setObjectName("TimelineView");

    connect(this, &TimelineView::setLastPositionTriggered, playbackController, [=](double tick) {
        playbackController->setLastPosition(tick);
        playbackController->setPosition(tick);
    });
    connect(playbackController, &PlaybackController::positionChanged, this,
            &TimelineView::setPosition);
    connect(appModel, &AppModel::modelChanged, this, [=] {
        setTimeSignature(appModel->timeSignature().numerator,
                         appModel->timeSignature().denominator);
    });
    connect(appModel, &AppModel::timeSignatureChanged, this, &TimelineView::setTimeSignature);
    connect(appStatus, &AppStatus::quantizeChanged, this, &TimelineView::setQuantize);
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

    // Draw playback indicator
    auto penWidth = 2.0;
    auto color = QColor(255, 204, 153);
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
    painter->drawText(QPointF(x + m_textPaddingLeft, 10), text);
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
    if (beat > 0)
        painter->drawText(QPointF(x + m_textPaddingLeft, 10),
                          /*QString::number(bar) + "." +*/ QString::number(beat));
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
    emit setLastPositionTriggered(xToTick(event->position().x()));
    // setPosition(xToTick(event->position().x()));
    event->ignore();
}

void TimelineView::mouseMoveEvent(QMouseEvent *event) {
    emit setLastPositionTriggered(xToTick(event->position().x()));
    // setPosition(xToTick(event->position().x()));
    QWidget::mouseMoveEvent(event);
}

void TimelineView::onPiecesChanged(const QList<InferPiece *> &pieces) {
    // for (const auto piece : m_pieces) {
    //     disconnect(piece, nullptr, this, nullptr);
    // }
    // for (const auto piece : pieces) {
    //     connect(piece, &InferPiece::statusChanged, this, [=](){});
    // }
    m_pieces = pieces;
    update();
}

void TimelineView::drawPieces(QPainter *painter) {
    auto penWidth = 2;
    auto y = rect().height() - penWidth;
    QPen pen;
    pen.setWidthF(2);
    pen.setColor(QColor(159, 189, 255));
    pen.setCapStyle(Qt::RoundCap);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    for (const auto &piece : m_clip->pieces()) {
        auto x1 = tickToX(piece->startTick() + m_clip->start());
        auto x2 = tickToX(piece->endTick() + m_clip->start());
        painter->drawLine(x1, y, x2, y);
        painter->drawText(QPointF(x1, y), "#" + QString::number(piece->id()));
    }
}

double TimelineView::tickToX(double tick) {
    auto ratio = (tick - m_startTick) / (m_endTick - m_startTick);
    auto x = rect().width() * ratio;
    return x;
}

double TimelineView::xToTick(double x) {
    auto tick = 1.0 * x / rect().width() * (m_endTick - m_startTick) + m_startTick;
    if (tick < 0)
        tick = 0;
    return tick;
}