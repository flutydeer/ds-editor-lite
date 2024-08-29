//
// Created by fluty on 24-8-28.
//

#ifndef SCROLLBARGRAPHICSITEM_H
#define SCROLLBARGRAPHICSITEM_H

#include "CommonGraphicsRectItem.h"
#include "UI/Utils/IAnimatable.h"

#include <QVariantAnimation>

class QScrollBar;

class ScrollBarGraphicsItem : public CommonGraphicsRectItem, public IAnimatable {
    Q_OBJECT

public:
    explicit ScrollBarGraphicsItem();
    explicit ScrollBarGraphicsItem(Qt::Orientation orientation);
    [[nodiscard]] Qt::Orientation orientation() const;
    void setOrientation(Qt::Orientation orientation);
    void updateRectAndPos() override;

protected:
    void afterSetAnimationLevel(AnimationGlobal::AnimationLevels level) override;
    void afterSetTimeScale(double scale) override;

private slots:
    void setHandleHoverAnimationValue(const QVariant &value);

private:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    // TODO: 在 CommonGraphicsView 里控制 hover 效果
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void initUi();
    [[nodiscard]] double handleStart() const;
    [[nodiscard]] double handleLength() const;
    [[nodiscard]] double handleEnd() const;
    void performHoverEnterAnimation();
    void performHoverLeaveAnimation();

    Qt::Orientation m_orientation = Qt::Horizontal;
    const int width = 14;
    const int handleAlphaHover = 64;
    const int handleAlphaPressed = 80;
    const int handleAlphaNormal = 40;
    QVariantAnimation m_statusAnimation; // Normal, Hover, Pressed
    bool m_mouseHoverOnHandle = false;

    double m_minimum = 0;
    double m_maximum = 100;
    double m_value = 20;
    double m_pageStep = 20;
    int m_statusAnimationValue = 127; // 0: Pressed; 127: Normal; 255: Hover
};



#endif // SCROLLBARGRAPHICSITEM_H
