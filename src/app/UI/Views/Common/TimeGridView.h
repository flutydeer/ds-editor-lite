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

private:
    friend class TimeGraphicsView;
    [[nodiscard]] double startTick() const;
    [[nodiscard]] double endTick() const;
    [[nodiscard]] double sceneXToTick(double pos) const;
    [[nodiscard]] double tickToSceneX(double tick) const;
    [[nodiscard]] double sceneXToItemX(double x) const;

    QColor barLineColor() const;
    void setBarLineColor(const QColor &color);
    QColor beatLineColor() const;
    void setBeatLineColor(const QColor &color);
    QColor commonLineColor() const;
    void setCommonLineColor(const QColor &color);

    int m_offset = 0;
    QColor m_barLineColor = {8, 9, 10};
    QColor m_beatLineColor = {22, 25, 28};
    QColor m_commonLineColor = {28, 32, 36};
};



#endif // TIMEGRIDVIEW_H
