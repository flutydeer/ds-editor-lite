//
// Created by fluty on 2024/2/3.
//

#ifndef TIMELINEVIEW_H
#define TIMELINEVIEW_H

#include <QWidget>
#include "Views/Utils/ITimelinePainter.h"

class TimelineView : public QWidget, public ITimelinePainter {
    Q_OBJECT

public slots:
    void setTimeRange(double startTick, double endTick);
    void setTimeSignature(int numerator, int denominator) override;
    void setPosition(double tick);
    void setQuantize(int quantize) override;

signals:
    void wheelHorScale(QWheelEvent *event);
    void setLastPositionTriggered(double tick);

protected:
    void paintEvent(QPaintEvent *event) override;
    void drawBar(QPainter *painter, int tick, int bar) override;
    void drawBeat(QPainter *painter, int tick, int bar, int beat) override;
    void drawEighth(QPainter *painter, int tick) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

    const QColor barLineColor = QColor(92, 96, 100);
    const QColor barTextColor = QColor(200, 200, 200);
    const QColor backgroundColor = QColor(42, 43, 44);
    const QColor beatLineColor = QColor(72, 75, 78);
    const QColor commonLineColor = QColor(57, 59, 61);
    const QColor beatTextColor = QColor(160, 160, 160);

    enum MouseMoveBehavior { SetPosition, SelectLoopRange };

private:
    double tickToX(double tick);
    double xToTick(double x);
    double m_startTick = 0;
    double m_endTick = 0;
    int m_textPaddingLeft = 2;
    double m_position = 0;
};



#endif // TIMELINEVIEW_H
