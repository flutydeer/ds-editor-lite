//
// Created by fluty on 2024/2/3.
//

#ifndef TIMEINDICATORGRAPHICSITEM_H
#define TIMEINDICATORGRAPHICSITEM_H

#include <QObject>
#include <QGraphicsLineItem>

class TimeIndicatorGraphicsItem : public QObject,public QGraphicsLineItem {
    Q_OBJECT

public:
    void setPixelsPerQuarterNote(int px);

public slots:
    void setPosition(double tick);
    void setScale(qreal sx, qreal sy);
    void setVisibleRect(const QRectF &rect);

private:
    double m_time = 0;
    double m_scaleX = 1.0;
    double m_pixelsPerQuarterNote = 64;
    QRectF m_visibleRect;

    void updateLengthAndPos();
    double tickToItemX(double tick) const;
};



#endif //TIMEINDICATORGRAPHICSITEM_H
