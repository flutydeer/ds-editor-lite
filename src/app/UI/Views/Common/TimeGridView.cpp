//
// Created by fluty on 2024/1/21.
//

#include "TimeGridView.h"

#include "Model/AppModel/AppModel.h"
#include "Model/AppStatus/AppStatus.h"

#include <QPainter>
#include <QPen>

TimeGridView::TimeGridView(QGraphicsItem *parent) : AbstractGraphicsRectItem(parent) {

    connect(appModel, &AppModel::modelChanged, this, [=] {
        this->setTimeSignature(appModel->timeSignature().numerator,
                               appModel->timeSignature().denominator);
        this->setQuantize(appStatus->quantize);
    });
    connect(appModel, &AppModel::timeSignatureChanged, this,
            &TimeGridView::setTimeSignature);
    connect(appStatus, &AppStatus::quantizeChanged, this, &TimeGridView::setQuantize);
}

double TimeGridView::startTick() const {
    return sceneXToTick(visibleRect().left()) + m_offset;
}

double TimeGridView::endTick() const {
    return sceneXToTick(visibleRect().right()) + m_offset;
}

void TimeGridView::setTimeSignature(int numerator, int denominator) {
    ITimelinePainter::setTimeSignature(numerator, denominator);
    update();
}

void TimeGridView::setQuantize(int quantize) {
    ITimelinePainter::setQuantize(quantize);
    update();
}

void TimeGridView::setOffset(int tick) {
    m_offset = tick;
    update();
}

void TimeGridView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                 QWidget *widget) {
    auto penWidth = 1;

    QPen pen;
    pen.setWidthF(penWidth);
    painter->setBrush(Qt::NoBrush);
    pen.setColor(m_commonLineColor);
    painter->setPen(pen);
    // painter->setRenderHint(QPainter::Antialiasing);
    drawTimeline(painter, startTick(), endTick(), visibleRect().width());
}

void TimeGridView::updateRectAndPos() {
    auto pos = visibleRect().topLeft();
    setPos(pos);
    setRect(QRectF(0, 0, visibleRect().width(), visibleRect().height()));
    update();
}

void TimeGridView::drawBar(QPainter *painter, int tick, int bar) {
    QPen pen;
    auto x = sceneXToItemX(tickToSceneX(tick - m_offset));
    // pen.setColor(barTextColor);
    // painter->setPen(pen);
    // painter->drawText(QPointF(x, 10), QString::number(bar));
    pen.setColor(m_barLineColor);
    painter->setPen(pen);
    painter->drawLine(QLineF(x, 0, x, visibleRect().height()));
}

void TimeGridView::drawBeat(QPainter *painter, int tick, int bar, int beat) {
    QPen pen;
    auto x = sceneXToItemX(tickToSceneX(tick - m_offset));
    // pen.setColor(beatTextColor);
    // painter->setPen(pen);
    // painter->drawText(QPointF(x, 10), QString::number(bar) + "." + QString::number(beat));
    pen.setColor(m_beatLineColor);
    painter->setPen(pen);
    painter->drawLine(QLineF(x, 0, x, visibleRect().height()));
}

void TimeGridView::drawEighth(QPainter *painter, int tick) {
    QPen pen;
    auto x = sceneXToItemX(tickToSceneX(tick - m_offset));
    pen.setColor(m_commonLineColor);
    painter->setPen(pen);
    painter->drawLine(QLineF(x, 0, x, visibleRect().height()));
}

double TimeGridView::sceneXToTick(double pos) const {
    return 480 * pos / scaleX() / pixelsPerQuarterNote();
}

double TimeGridView::tickToSceneX(double tick) const {
    return tick * scaleX() * pixelsPerQuarterNote() / 480;
}

double TimeGridView::sceneXToItemX(double x) const {
    return mapFromScene(QPointF(x, 0)).x();
}

QColor TimeGridView::barLineColor() const {
    return m_barLineColor;
}

void TimeGridView::setBarLineColor(const QColor &color) {
    m_barLineColor = color;
    update();
}

QColor TimeGridView::beatLineColor() const {
    return m_beatLineColor;
}

void TimeGridView::setBeatLineColor(const QColor &color) {
    m_beatLineColor = color;
    update();
}

QColor TimeGridView::commonLineColor() const {
    return m_commonLineColor;
}

void TimeGridView::setCommonLineColor(const QColor &color) {
    m_commonLineColor = color;
    update();
}
