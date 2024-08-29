//
// Created by fluty on 24-8-28.
//

#include "ScrollBarGraphicsItem.h"

#include "CommonGraphicsScene.h"
#include "Global/AppGlobal.h"

#include <QPainter>
#include <QGraphicsSceneHoverEvent>
#include <QCursor>

ScrollBarGraphicsItem::ScrollBarGraphicsItem() {
    initUi();
}

ScrollBarGraphicsItem::ScrollBarGraphicsItem(Qt::Orientation orientation)
    : m_orientation(orientation) {
    initUi();
}

Qt::Orientation ScrollBarGraphicsItem::orientation() const {
    return m_orientation;
}

void ScrollBarGraphicsItem::setOrientation(Qt::Orientation orientation) {
    m_orientation = orientation;
    updateRectAndPos();
}

void ScrollBarGraphicsItem::updateRectAndPos() {
    if (!scene())
        return;

    if (m_orientation == Qt::Horizontal) {
        m_maximum = scene()->sceneRect().width();
        m_value = visibleRect().left();
        m_pageStep = visibleRect().width();
        setPos(visibleRect().left(), visibleRect().bottom() - width);
        setRect(QRectF(0, 0, visibleRect().width() - width, width));
    } else {
        m_maximum = scene()->sceneRect().height();
        m_value = visibleRect().top();
        m_pageStep = visibleRect().height();
        setPos(visibleRect().right() - width, visibleRect().top());
        setRect(QRectF(0, 0, width, visibleRect().height() - width));
    }
    update();
}

void ScrollBarGraphicsItem::afterSetAnimationLevel(AnimationGlobal::AnimationLevels level) {
}

void ScrollBarGraphicsItem::afterSetTimeScale(double scale) {
}

void ScrollBarGraphicsItem::setHandleHoverAnimationValue(const QVariant &value) {
    m_statusAnimationValue = value.toInt();
    update();
}

void ScrollBarGraphicsItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                  QWidget *widget) {
    // if (m_pageStep >= (m_maximum - m_minimum))
    //     return;

    painter->setRenderHint(QPainter::Antialiasing);
    // TODO: fix status animation
    auto aniRatio = (m_statusAnimationValue - 127) / (255.0 - 127);
    int alpha = handleAlphaNormal + qRound(aniRatio * (handleAlphaHover - handleAlphaNormal));
    const auto backgroundColor = QColor(255, 255, 255, alpha);
    // const auto backgroundColor = QColor(32 + alpha, 32 + alpha, 32 + alpha);
    const auto radiusBase = 2;
    auto padding = 5 - aniRatio * 3;

    QRectF handleRect;
    if (m_orientation == Qt::Horizontal) {
        auto left = handleStart();
        auto top = boundingRect().top() + padding;
        auto width = handleLength();
        auto height = boundingRect().height() - padding * 2;
        handleRect = QRectF(left, top, width, height);
    } else {
        auto left = boundingRect().left() + padding;
        auto top = handleStart();
        auto width = boundingRect().width() - padding * 2;
        auto height = handleLength();
        handleRect = QRectF(left, top, width, height);
    }
    const auto radiusX = handleRect.width() / 2 >= radiusBase ? radiusBase : handleRect.width() / 2;
    const auto radiusY =
        handleRect.height() / 2 >= radiusBase ? radiusBase : handleRect.height() / 2;
    const auto radius = std::min(radiusX, radiusY);

    painter->setPen(Qt::NoPen);
    painter->setBrush(backgroundColor);
    painter->drawRoundedRect(handleRect, radius, radius);
}

void ScrollBarGraphicsItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    // qDebug() << "hover enter";
    if (m_orientation == Qt::Horizontal) {
        auto x = event->pos().x();
        if (x > handleStart() && x < handleEnd()) {
            m_mouseHoverOnHandle = true;
            performHoverEnterAnimation();
        }
    } else {
        auto y = event->pos().y();
        if (y > handleStart() && y < handleEnd()) {
            m_mouseHoverOnHandle = true;
            performHoverEnterAnimation();
        }
    }
    CommonGraphicsRectItem::hoverEnterEvent(event);
}

void ScrollBarGraphicsItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    // const auto x = event->pos().x();
    // auto start = handleStart();
    // auto end = handleEnd();
    // if (x >= start && x <= start + AppGlobal::resizeTolarance ||
    //     x >= end - AppGlobal::resizeTolarance && x <= end)
    //     setCursor(Qt::SizeHorCursor);
    // else
    //     setCursor(Qt::ArrowCursor);

    if (m_orientation == Qt::Horizontal) {
        auto x = event->pos().x();
        if (x > handleStart() && x < handleEnd()) {
            if (!m_mouseHoverOnHandle)
                performHoverEnterAnimation();
            m_mouseHoverOnHandle = true;
        } else {
            m_mouseHoverOnHandle = false;
            performHoverLeaveAnimation();
        }
    } else {
        auto y = event->pos().y();
        if (y > handleStart() && y < handleEnd()) {
            if (!m_mouseHoverOnHandle)
                performHoverEnterAnimation();
            m_mouseHoverOnHandle = true;
        } else {
            m_mouseHoverOnHandle = false;
            performHoverLeaveAnimation();
        }
    }

    CommonGraphicsRectItem::hoverMoveEvent(event);
}

void ScrollBarGraphicsItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    // qDebug() << "hover leave";
    if (m_mouseHoverOnHandle)
        performHoverLeaveAnimation();
    m_mouseHoverOnHandle = false;
    CommonGraphicsRectItem::hoverLeaveEvent(event);
}

void ScrollBarGraphicsItem::initUi() {
    setAcceptHoverEvents(true);
    updateRectAndPos();
    initializeAnimation();

    m_statusAnimation.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_statusAnimation, &QVariantAnimation::valueChanged, this,
            &ScrollBarGraphicsItem::setHandleHoverAnimationValue);
}

double ScrollBarGraphicsItem::handleStart() const {
    auto ratio = (m_value - m_minimum) / (m_maximum - m_minimum);
    if (m_orientation == Qt::Horizontal)
        return boundingRect().left() + boundingRect().width() * ratio;
    return boundingRect().top() + boundingRect().height() * ratio; // Vertical
}

double ScrollBarGraphicsItem::handleLength() const {
    if (m_orientation == Qt::Horizontal)
        return boundingRect().width() * m_pageStep / (m_maximum - m_minimum);
    return boundingRect().height() * m_pageStep / (m_maximum - m_minimum);
}

double ScrollBarGraphicsItem::handleEnd() const {
    return handleStart() + handleLength();
}

void ScrollBarGraphicsItem::performHoverEnterAnimation() {
    m_statusAnimation.stop();
    m_statusAnimation.setDuration(static_cast<int>(animationTimeScale() * 100));
    m_statusAnimation.setStartValue(m_statusAnimationValue);
    m_statusAnimation.setEndValue(255);
    m_statusAnimation.start();
}

void ScrollBarGraphicsItem::performHoverLeaveAnimation() {
    m_statusAnimation.stop();
    m_statusAnimation.setDuration(static_cast<int>(animationTimeScale() * 400));
    m_statusAnimation.setStartValue(m_statusAnimationValue);
    m_statusAnimation.setEndValue(127);
    m_statusAnimation.start();
}