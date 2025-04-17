//
// Created by FlutyDeer on 2025/4/1.
//

#include "AnimationView.h"

#include <QPainter>
#include <QHoverEvent>

AnimationView::AnimationView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);
    setAttribute(Qt::WA_Hover, true);

    connect(&m_animator, &ElasticAnimator::positionUpdated, this,
            &AnimationView::onPositionUpdated);

    setStyleSheet("background-color: rgb( 39, 42, 51);");
}

void AnimationView::onPositionUpdated(QPointF position, QPointF velocity) {
    m_objectPosition = position;
    qDebug() << position << velocity;
    update();
}

void AnimationView::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(155, 186, 255));

    auto radius = 20.0;
    painter.drawEllipse(m_objectPosition, radius, radius);
}

bool AnimationView::event(QEvent *event) {
    if (event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverMove) {
        auto hoverEvent = static_cast<QHoverEvent *>(event);
        m_animator.setTarget(hoverEvent->position());
        update();
    }
    return QWidget::event(event);
}