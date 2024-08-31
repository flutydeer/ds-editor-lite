//
// Created by fluty on 24-8-28.
//

#include "ScrollBarGraphicsItem.h"

#include "CommonGraphicsScene.h"
#include "Global/AppGlobal.h"

#include <QPainter>

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

void ScrollBarGraphicsItem::moveToNormalState() {
    performStateChangeAnimation(handleAlphaNormal, handlePaddingNormal, 300);
}

void ScrollBarGraphicsItem::moveToHoverState() {
    performStateChangeAnimation(handleAlphaHover, handlePaddingHover, 100);
}

void ScrollBarGraphicsItem::moveToPressedState() {
    performStateChangeAnimation(handleAlphaPressed, handlePaddingPressed, 100);
}

bool ScrollBarGraphicsItem::mouseOnHandle(const QPointF &scenePos) const {
    if (m_orientation == Qt::Horizontal) {
        auto x = scenePos.x();
        if (x > handleStart() && x < handleEnd())
            return true;
    } else {
        auto y = scenePos.y();
        if (y > handleStart() && y < handleEnd())
            return true;
    }
    return false;
}

void ScrollBarGraphicsItem::afterSetAnimationLevel(AnimationGlobal::AnimationLevels level) {
}

void ScrollBarGraphicsItem::afterSetTimeScale(double scale) {
}

void ScrollBarGraphicsItem::setHandleAlpha(const QVariant &value) {
    m_handleAlpha = value.toInt();
    update();
}

void ScrollBarGraphicsItem::setHandlePadding(const QVariant &value) {
    m_handlePadding = value.toDouble();
    update();
}

void ScrollBarGraphicsItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                  QWidget *widget) {
    // if (m_pageStep >= (m_maximum - m_minimum))
    //     return;

    painter->setRenderHint(QPainter::Antialiasing);
    const auto backgroundColor = QColor(255, 255, 255, m_handleAlpha);
    const auto radiusBase = 2;
    auto padding = m_handlePadding;

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

void ScrollBarGraphicsItem::initUi() {
    updateRectAndPos();
    initializeAnimation();

    m_aniHandleAlpha.setEasingCurve(QEasingCurve::OutCubic);
    m_aniHandlePadding.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_aniHandleAlpha, &QVariantAnimation::valueChanged, this,
            &ScrollBarGraphicsItem::setHandleAlpha);
    connect(&m_aniHandlePadding, &QVariantAnimation::valueChanged, this,
            &ScrollBarGraphicsItem::setHandlePadding);
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

void ScrollBarGraphicsItem::performStateChangeAnimation(int targetAlpha, double targetPadding,
                                                        int duration) {
    m_aniHandleAlpha.stop();
    m_aniHandlePadding.stop();

    m_aniHandleAlpha.setDuration(static_cast<int>(animationTimeScale() * duration));
    m_aniHandlePadding.setDuration(static_cast<int>(animationTimeScale() * duration));

    m_aniHandleAlpha.setStartValue(m_handleAlpha);
    m_aniHandlePadding.setStartValue(m_handlePadding);

    m_aniHandleAlpha.setEndValue(targetAlpha);
    m_aniHandlePadding.setEndValue(targetPadding);

    m_aniHandleAlpha.start();
    m_aniHandlePadding.start();
}