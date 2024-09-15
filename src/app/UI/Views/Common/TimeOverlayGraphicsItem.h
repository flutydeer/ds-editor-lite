//
// Created by fluty on 2024/1/25.
//

#ifndef OVERLAYGRAPHICSITEM_H
#define OVERLAYGRAPHICSITEM_H

#include "CommonGraphicsRectItem.h"

class TimeOverlayGraphicsItem : public CommonGraphicsRectItem {
    Q_OBJECT

public:
    [[nodiscard]] bool transparentMouseEvents() const;
    void setTransparentMouseEvents(bool on);
    void setPixelsPerQuarterNote(int p);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

    // Time methods
    [[nodiscard]] double startTick() const;
    [[nodiscard]] double endTick() const;
    [[nodiscard]] double sceneXToTick(double x) const;
    [[nodiscard]] double tickToSceneX(double tick) const;
    [[nodiscard]] double sceneXToItemX(double x) const;
    [[nodiscard]] double tickToItemX(double tick) const;
    [[nodiscard]] double sceneYToItemY(double y) const;

private:
    bool m_transparentMouseEvents = true;
    int pixelsPerQuarterNote = 96;
};

#endif // OVERLAYGRAPHICSITEM_H
