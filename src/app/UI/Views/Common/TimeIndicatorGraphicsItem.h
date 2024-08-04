//
// Created by fluty on 2024/2/3.
//

#ifndef TIMEINDICATORGRAPHICSITEM_H
#define TIMEINDICATORGRAPHICSITEM_H

#include <QObject>
#include <QGraphicsLineItem>

#include "UI/Utils/IScalableItem.h"

class TimeIndicatorGraphicsItem : public QObject, public QGraphicsLineItem, public IScalableItem {
    Q_OBJECT

public:
    explicit TimeIndicatorGraphicsItem(QObject *parent = nullptr);
    void setPixelsPerQuarterNote(int px);

public slots:
    void setPosition(double tick);
    void setOffset(int tick);

protected:
    void afterSetScale() override;
    void afterSetVisibleRect() override;

private:
    double m_time = 0;
    double m_pixelsPerQuarterNote = 64;
    int m_offset = 0;

    void updateLengthAndPos();
    [[nodiscard]] double tickToItemX(double tick) const;
};

#endif // TIMEINDICATORGRAPHICSITEM_H
