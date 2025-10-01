//
// Created by fluty on 2024/1/25.
//

#ifndef TIMEOVERLAYVIEW
#define TIMEOVERLAYVIEW

#include "AbstractGraphicsRectItem.h"

class TimeOverlayView : public AbstractGraphicsRectItem {
    Q_OBJECT

public:
    bool transparentMouseEvents() const;
    void setTransparentMouseEvents(bool on);
    void setPixelsPerQuarterNote(int p);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

    // Time methods
    double startTick() const;
    double endTick() const;
    double sceneXToTick(double x) const;
    double tickToSceneX(double tick) const;
    double sceneXToItemX(double x) const;
    double tickToItemX(double tick) const;
    double sceneYToItemY(double y) const;

private:
    bool m_transparentMouseEvents = true;
    int pixelsPerQuarterNote = 96;
};

#endif // TIMEOVERLAYVIEW
