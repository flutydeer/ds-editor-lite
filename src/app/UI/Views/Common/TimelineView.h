//
// Created by fluty on 2024/2/3.
//

#ifndef TIMELINEVIEW_H
#define TIMELINEVIEW_H

#include <QWidget>
#include <QTimer>
#include "UI/Utils/ITimelinePainter.h"

class SingingClip;
class InferPiece;
class LoopSettings;

class TimelineView : public QWidget, public ITimelinePainter {
    Q_OBJECT
    Q_PROPERTY(QColor playheadColor READ playheadColor WRITE setPlayheadColor)
    Q_PROPERTY(QColor barScaleColor READ barScaleColor WRITE setBarScaleColor)
    Q_PROPERTY(QColor barTickColor READ barTickColor WRITE setBarTickColor)
    Q_PROPERTY(QColor beatScaleColor READ beatScaleColor WRITE setBeatScaleColor)
    Q_PROPERTY(QColor beatTickColor READ beatTickColor WRITE setBeatTickColor)
    Q_PROPERTY(QColor subdivisionFromColor READ subdivisionFromColor WRITE setSubdivisionFromColor)
    Q_PROPERTY(QColor subdivisionToColor READ subdivisionToColor WRITE setSubdivisionToColor)
    Q_PROPERTY(QColor loopMarkerColor READ loopMarkerColor WRITE setLoopMarkerColor)
    Q_PROPERTY(
        QColor loopMarkerDisabledColor READ loopMarkerDisabledColor WRITE setLoopMarkerDisabledColor)
    Q_PROPERTY(QColor piecePendingColor READ piecePendingColor WRITE setPiecePendingColor)
    Q_PROPERTY(QColor pieceRunningColor READ pieceRunningColor WRITE setPieceRunningColor)
    Q_PROPERTY(QColor pieceSuccessColor READ pieceSuccessColor WRITE setPieceSuccessColor)
    Q_PROPERTY(QColor pieceFailedColor READ pieceFailedColor WRITE setPieceFailedColor)

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
    void drawSubdivision(QPainter *painter, int tick, int level, int levelCount) override;
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
    void drawPieceDebugOverlay(QPainter *painter, const InferPiece *piece) const;
    void drawLoopRegion(QPainter *painter) const;
    void drawLoopBackground(QPainter *painter) const;
    void drawLoopMarkers(QPainter *painter) const;
    double tickToX(double tick) const;
    double xToTick(double x) const;
    void updateCursor(const QPoint &pos);

    // Theme color accessors (QSS-overridable via qproperty-*)
    [[nodiscard]] QColor playheadColor() const;
    void setPlayheadColor(const QColor &color);
    [[nodiscard]] QColor barScaleColor() const;
    void setBarScaleColor(const QColor &color);
    [[nodiscard]] QColor barTickColor() const;
    void setBarTickColor(const QColor &color);
    [[nodiscard]] QColor beatScaleColor() const;
    void setBeatScaleColor(const QColor &color);
    [[nodiscard]] QColor beatTickColor() const;
    void setBeatTickColor(const QColor &color);
    [[nodiscard]] QColor subdivisionFromColor() const;
    void setSubdivisionFromColor(const QColor &color);
    [[nodiscard]] QColor subdivisionToColor() const;
    void setSubdivisionToColor(const QColor &color);
    [[nodiscard]] QColor loopMarkerColor() const;
    void setLoopMarkerColor(const QColor &color);
    [[nodiscard]] QColor loopMarkerDisabledColor() const;
    void setLoopMarkerDisabledColor(const QColor &color);
    [[nodiscard]] QColor piecePendingColor() const;
    void setPiecePendingColor(const QColor &color);
    [[nodiscard]] QColor pieceRunningColor() const;
    void setPieceRunningColor(const QColor &color);
    [[nodiscard]] QColor pieceSuccessColor() const;
    void setPieceSuccessColor(const QColor &color);
    [[nodiscard]] QColor pieceFailedColor() const;
    void setPieceFailedColor(const QColor &color);

    enum LoopDragMode { None, DragStart, DragEnd, DragBody };

    LoopDragMode hitTestLoop(const QPoint &pos) const;

    double m_startTick = 0;
    double m_endTick = 0;
    int m_textPaddingLeft = 2;
    double m_position = 0;
    QList<InferPiece *> m_pieces;
    SingingClip *m_clip = nullptr;
    // Piece status colors: Pending, Running, Success, Failed (indexed by status)
    QList<QColor> m_piecesColors = {QColor(100, 100, 100), QColor(255, 204, 153),
                                    QColor(155, 255, 162), QColor(255, 155, 157)};

    // Theme colors (QSS-overridable via qproperty-*)
    QColor m_playheadColor = {200, 200, 200};
    QColor m_barScaleColor = {200, 200, 200};
    QColor m_barTickColor = {92, 96, 100};
    QColor m_beatScaleColor = {160, 160, 160};
    QColor m_beatTickColor = {72, 75, 78};
    QColor m_subdivisionFromColor = {76, 79, 83};
    QColor m_subdivisionToColor = {57, 59, 61};
    QColor m_loopMarkerColor = {155, 186, 255};
    QColor m_loopMarkerDisabledColor = {57, 59, 61};

    // Loop region
    bool m_canEditLoop = false;
    LoopDragMode m_loopDragMode = None;
    int m_loopDragStartTick = 0;
    int m_loopDragStartPos = 0;
    int m_loopRegionHeight = 10;
    int m_loopHandleWidth = 8;
    QTimer m_pieceUpdateThrottle;
    QTimer m_positionThrottle;
    double m_pendingPosition = 0;
};



#endif // TIMELINEVIEW_H
