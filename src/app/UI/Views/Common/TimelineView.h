//
// Created by fluty on 2024/2/3.
//

#ifndef TIMELINEVIEW_H
#define TIMELINEVIEW_H

#include <QWidget>
#include "Model/AppModel/LoopSettings.h"
#include "UI/Utils/ITimelinePainter.h"

class SingingClip;
class InferPiece;

class TimelineView : public QWidget, public ITimelinePainter {
    Q_OBJECT

public:
    explicit TimelineView(QWidget *parent = nullptr);

    void setCanEditLoop(bool canEdit);

public slots:
    void setTimeRange(double startTick, double endTick);
    void setTimeSignature(int numerator, int denominator) override;
    void setPosition(double tick);
    void setQuantize(int quantize) override;
    void setDataContext(SingingClip *clip);

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
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

    enum MouseMoveBehavior { SetPosition, SelectLoopRange };

private slots:
    void onPiecesChanged(const QList<InferPiece *> &pieces);
    void onLoopSettingsChanged(const LoopSettings &settings);

private:
    void drawPieces(QPainter *painter) const;
    void drawLoopRegion(QPainter *painter) const;
    double tickToX(double tick) const;
    double xToTick(double x) const;
    void cacheText(const QString &type, const QString &text, const QPainter &painter);
    void updateCursor(const QPoint &pos);

    enum LoopDragMode { None, DragStart, DragEnd, DragBody };
    LoopDragMode hitTestLoop(const QPoint &pos) const;

    double m_startTick = 0;
    double m_endTick = 0;
    int m_textPaddingLeft = 2;
    double m_position = 0;
    QList<InferPiece *> m_pieces;
    SingingClip *m_clip = nullptr;
    const QList<QColor> m_piecesColors = {
        QColor(100, 100, 100), QColor(255, 204, 153), QColor(155, 255, 162),
        QColor(255, 155, 157)}; // Pending, Running, Success, Failed
    QMap<QString, QMap<QString, QPixmap>> m_textCache;

    // Loop region
    bool m_canEditLoop = false;
    LoopDragMode m_loopDragMode = None;
    int m_loopDragStartTick = 0;
    int m_loopDragStartPos = 0;
    LoopSettings m_loopDragStartSettings;
    int m_loopRegionHeight = 10;
    int m_loopHandleWidth = 8;
};



#endif // TIMELINEVIEW_H
