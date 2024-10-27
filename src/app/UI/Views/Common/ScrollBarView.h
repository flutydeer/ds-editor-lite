//
// Created by fluty on 24-8-28.
//

#ifndef SCROLLBARVIEW_H
#define SCROLLBARVIEW_H

#include "AbstractGraphicsRectItem.h"
#include "UI/Utils/IAnimatable.h"

#include <QVariantAnimation>

class QScrollBar;

class ScrollBarView : public AbstractGraphicsRectItem, public IAnimatable {
    Q_OBJECT

public:
    explicit ScrollBarView();
    explicit ScrollBarView(Qt::Orientation orientation);
    [[nodiscard]] Qt::Orientation orientation() const;
    void setOrientation(Qt::Orientation orientation);
    void updateRectAndPos() override;
    void moveToNormalState();
    void moveToHoverState();
    void moveToPressedState();
    [[nodiscard]] bool mouseOnHandle(const QPointF &scenePos) const;

protected:
    void afterSetAnimationLevel(AnimationGlobal::AnimationLevels level) override;
    void afterSetTimeScale(double scale) override;

private slots:
    void setHandleAlpha(const QVariant &value);
    void setHandlePadding(const QVariant &value);

private:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void initUi();
    [[nodiscard]] double handleStart() const;
    [[nodiscard]] double handleLength() const;
    [[nodiscard]] double handleEnd() const;
    void performStateChangeAnimation(int targetAlpha, double targetPadding, int duration);

    Qt::Orientation m_orientation = Qt::Horizontal;
    const int width = 14;
    const int handleAlphaHover = 64;
    const int handleAlphaPressed = 96;
    const int handleAlphaNormal = 40;
    const double handlePaddingNormal = 5;
    const double handlePaddingHover = 2;
    const double handlePaddingPressed = 3;
    QVariantAnimation m_aniHandleAlpha;
    QVariantAnimation m_aniHandlePadding;
    bool m_mouseHoverOnHandle = false;

    double m_minimum = 0;
    double m_maximum = 100;
    double m_value = 20;
    double m_pageStep = 20;
    int m_handleAlpha = handleAlphaNormal;
    double m_handlePadding = handlePaddingNormal;
};



#endif // SCROLLBARVIEW_H
