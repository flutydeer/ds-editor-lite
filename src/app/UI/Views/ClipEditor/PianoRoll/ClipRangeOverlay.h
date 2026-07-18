//
// Created by fluty on 24-9-16.
//

#ifndef CLIPRANGEOVERLAY_H
#define CLIPRANGEOVERLAY_H

#include "UI/Views/Common/TimeOverlayView.h"

#include <QColor>

class ClipRangeOverlay : public TimeOverlayView {
    Q_OBJECT

public:
    ClipRangeOverlay();
    void setClipRange(int clipStart, int clipLen);

    [[nodiscard]] QColor fillColor() const;
    void setFillColor(const QColor &color);

protected:
    void updateRectAndPos() override;

private:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    int m_clipStart = 0;
    int m_clipLen = 0;
    QColor m_fillColor = {0, 0, 0, 64};
};



#endif // CLIPRANGEOVERLAY_H
