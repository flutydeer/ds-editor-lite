//
// Created by fluty on 2024/1/21.
//

#ifndef TIMEGRIDGRAPHICSITEM_H
#define TIMEGRIDGRAPHICSITEM_H

#include "CommonGraphicsRectItem.h"
#include "Controls/Utils/ITimelinePainter.h"

class TimeGridGraphicsItem : public CommonGraphicsRectItem, public ITimelinePainter {
    Q_OBJECT

public:
    // class TimeSignature {
    // public:
    //     int position = 0;
    //     int numerator = 4;
    //     int denominator = 4;
    // };

    explicit TimeGridGraphicsItem(QGraphicsItem *parent = nullptr);
    ~TimeGridGraphicsItem() override = default;

    double startTick() const;
    double endTick() const;

signals:
    void timeRangeChanged(double startTick, double endTick);

public slots:
    void setTimeSignature(int numerator, int denominator) override;
    void setQuantize(int quantize) override;
    //     void onTimelineChanged();

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void updateRectAndPos() override;
    void drawBar(QPainter *painter, int tick, int bar) override;
    void drawBeat(QPainter *painter, int tick, int bar, int beat) override;
    void drawEighth(QPainter *painter, int tick) override;

    const QColor barLineColor = QColor(92, 96, 100);
    const QColor barTextColor = QColor(200, 200, 200);
    const QColor backgroundColor = QColor(42, 43, 44);
    const QColor beatLineColor = QColor(72, 75, 78);
    const QColor commonLineColor = QColor(57, 59, 61);
    const QColor beatTextColor = QColor(160, 160, 160);

private:
    double sceneXToTick(double pos) const;
    double tickToSceneX(double tick) const;
    double sceneXToItemX(double x) const;
};



#endif // TIMEGRIDGRAPHICSITEM_H
