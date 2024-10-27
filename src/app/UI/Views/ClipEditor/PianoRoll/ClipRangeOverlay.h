//
// Created by fluty on 24-9-16.
//

#ifndef CLIPRANGEOVERLAY_H
#define CLIPRANGEOVERLAY_H

#include "UI/Views/Common/TimeOverlayView.h"

class ClipRangeOverlay : public TimeOverlayView {
    Q_OBJECT

public:
    ClipRangeOverlay();
    void setClipRange(int clipStart, int clipLen);

protected:
    void updateRectAndPos() override;

private:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    int m_clipStart = 0;
    int m_clipLen = 0;
};



#endif // CLIPRANGEOVERLAY_H
