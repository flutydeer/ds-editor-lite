//
// Created by fluty on 2024/1/21.
//

#ifndef TIMEGRIDGRAPHICSITEM_H
#define TIMEGRIDGRAPHICSITEM_H

#include "CommonGraphicsRectItem.h"
#include "Controls/Utils/ITimelinePainter.h"

class TimeGridGraphicsItem : public CommonGraphicsRectItem, ITimelinePainter {
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

    void setPixelsPerQuarterNote(int px);

public slots:
    void onTimeSignatureChanged(int numerator, int denominator);
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
    double sceneXToTick(double pos);
    double tickToSceneX(double tick);
    double sceneXToItemX(double x);

    int m_numerator = 4;
    int m_denominator = 4;
    int m_minimumSpacing = 24;
    int m_pixelsPerQuarterNote = 64;
};



#endif // TIMEGRIDGRAPHICSITEM_H
