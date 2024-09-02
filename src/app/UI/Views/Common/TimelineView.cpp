//
// Created by fluty on 2024/2/3.
//

#include "TimelineView.h"

#include <QPainter>
#include <QWheelEvent>

#include "Controller/PlaybackController.h"
#include "Global/AppGlobal.h"
#include "Model/AppStatus/AppStatus.h"

using namespace AppGlobal;

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
    drawTimeline(&painter, m_startTick, m_endTick,
                 rect().width());

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
    pen.setColor(barTextColor);
    painter->setPen(pen);
    painter->drawText(QPointF(x + m_textPaddingLeft, 10), QString::number(bar));
    pen.setColor(barLineColor);
    painter->setPen(pen);
    auto y1 = 0;
    auto y2 = rect().height();
    painter->drawLine(QLineF(x, y1, x, y2));
}

void TimelineView::drawBeat(QPainter *painter, int tick, int bar, int beat) {
    QPen pen;
    auto x = tickToX(tick);
    pen.setColor(beatTextColor);
    painter->setPen(pen);
    painter->drawText(QPointF(x + m_textPaddingLeft, 10),
                      QString::number(bar) + "." + QString::number(beat));
    pen.setColor(beatLineColor);
    painter->setPen(pen);
    auto y1 = 2.0 / 3 * rect().height();
    auto y2 = rect().height();
    painter->drawLine(QLineF(x, y1, x, y2));
}

void TimelineView::drawEighth(QPainter *painter, int tick) {
    QPen pen;
    auto x = tickToX(tick);
    pen.setColor(commonLineColor);
    painter->setPen(pen);
    auto y1 = 3.0 / 4 * rect().height();
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

double TimelineView::tickToX(double tick) {
    auto ratio = (tick - m_startTick) / (m_endTick - m_startTick);
    auto x = rect().width() * ratio;
    return x;
}

double TimelineView::xToTick(double x) {
    auto tick =
        1.0 * x / rect().width() * (m_endTick - m_startTick) +
        m_startTick;
    if (tick < 0)
        tick = 0;
    return tick;
}