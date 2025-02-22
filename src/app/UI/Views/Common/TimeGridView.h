//
// Created by fluty on 2024/1/21.
//

#ifndef TIMEGRIDVIEW_H
#define TIMEGRIDVIEW_H

#include "AbstractGraphicsRectItem.h"
#include "UI/Utils/ITimelinePainter.h"

class TimeGridView : public AbstractGraphicsRectItem, public ITimelinePainter {
    Q_OBJECT

public:
    // class TimeSignature {
    // public:
    //     int position = 0;
    //     int numerator = 4;
    //     int denominator = 4;
    // };

    explicit TimeGridView(QGraphicsItem *parent = nullptr);
    ~TimeGridView() override = default;

public slots:
    void setTimeSignature(int numerator, int denominator) override;
    void setQuantize(int quantize) override;
    void setOffset(int tick);
    //     void onTimelineChanged();

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void updateRectAndPos() override;
    void drawBar(QPainter *painter, int tick, int bar) override;
    void drawBeat(QPainter *painter, int tick, int bar, int beat) override;
    void drawEighth(QPainter *painter, int tick) override;

    const QColor barLineColor = QColor(25, 28, 33);
    const QColor barTextColor = QColor(200, 200, 200);
    const QColor backgroundColor = QColor(42, 43, 44);
    const QColor beatLineColor = QColor(31, 35, 41);
    const QColor commonLineColor = QColor(33, 38, 43);
    const QColor beatTextColor = QColor(160, 160, 160);

private:
    [[nodiscard]] double startTick() const;
    [[nodiscard]] double endTick() const;
    [[nodiscard]] double sceneXToTick(double pos) const;
    [[nodiscard]] double tickToSceneX(double tick) const;
    [[nodiscard]] double sceneXToItemX(double x) const;

    int m_offset = 0;
};



#endif // TIMEGRIDVIEW_H
