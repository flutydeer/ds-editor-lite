//
// Created by fluty on 2024/2/3.
//

#ifndef TIMELINEVIEW_H
#define TIMELINEVIEW_H

#include <QWidget>
#include "UI/Utils/ITimelinePainter.h"

class SingingClip;
class InferPiece;

class TimelineView : public QWidget, public ITimelinePainter {
    Q_OBJECT

public:
    explicit TimelineView(QWidget *parent = nullptr);

public slots:
    void setTimeRange(double startTick, double endTick);
    void setTimeSignature(int numerator, int denominator) override;
    void setPosition(double tick);
    void setQuantize(int quantize) override;
    void setDataContext(SingingClip *clip);
    // void setPieces(const QList<InferPiece *> &pieces);

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

    enum MouseMoveBehavior { SetPosition, SelectLoopRange };

private slots:
    void onPiecesChanged(const QList<InferPiece *> &pieces);

private:
    void drawPieces(QPainter *painter);
    double tickToX(double tick);
    double xToTick(double x);
    double m_startTick = 0;
    double m_endTick = 0;
    int m_textPaddingLeft = 2;
    double m_position = 0;
    QList<InferPiece *> m_pieces;
    SingingClip *m_clip = nullptr;
};



#endif // TIMELINEVIEW_H
