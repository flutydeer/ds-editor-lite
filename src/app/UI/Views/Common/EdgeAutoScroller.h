//
// Edge auto scroll for editor views.
//
// Pure computation (speed curve, sub-pixel accumulation, pointer clamping) is
// kept free of any widget dependency so it can be unit-tested with injected
// frame times. The runner part is a plain QTimer + QElapsedTimer pair that
// reports real frame delta times through the frame() signal; the owning view
// is responsible for querying the pointer, applying the scroll delta to its
// scroll bars and continuing the active drag.
//

#ifndef EDGEAUTOSCROLLER_H
#define EDGEAUTOSCROLLER_H

#include <QElapsedTimer>
#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QTimer>

struct EdgeAutoScrollConfig {
    int hotZoneH = 72;       // horizontal hot zone width in viewport pixels
    int hotZoneV = 56;       // vertical hot zone height in viewport pixels
    double maxSpeedH = 1200; // px/s at full horizontal strength
    double maxSpeedV = 800;  // px/s at full vertical strength
    double baseSpeed = 80;   // px/s at the inner hot zone boundary
    int intervalMs = 16;     // timer interval
};

class EdgeAutoScroller : public QObject {
    Q_OBJECT

public:
    explicit EdgeAutoScroller(QObject *parent = nullptr);

    [[nodiscard]] const EdgeAutoScrollConfig &config() const;
    void setConfig(const EdgeAutoScrollConfig &config);

    // --- Pure computation (testable, no timer involved) ---

    // Signed speed (px/s) along one axis. Each edge carries its own hot zone
    // width. pos may lie outside [start, end]; in that case the speed
    // saturates at +-maxSpeed. Overlapping hot zones (tiny viewport) produce
    // opposing contributions that cancel symmetrically.
    static double axisSpeed(double pos, double start, double end, double lowHotZone,
                            double highHotZone, double maxSpeed, double baseSpeed);

    // Velocity vector (px/s) for the raw (unclamped) pointer position.
    // Disabled axes always yield zero.
    static QPointF velocity(const QPointF &pointerPos, const QRectF &viewportRect,
                            Qt::Orientations axes, const EdgeAutoScrollConfig &config);

    // Velocity for a drag that started at pressPos (same coordinate space as
    // pointerPos). For every edge, if the pointer already lay inside the hot
    // zone at press time, that edge's zone is narrowed to the press distance,
    // so scrolling only starts when the pointer moves closer to the edge than
    // it was when the drag began (prevents accidental scroll-back when a drag
    // starts near a corner and moves toward the viewport center).
    static QPointF velocity(const QPointF &pointerPos, const QPointF &pressPos,
                            const QRectF &viewportRect, Qt::Orientations axes,
                            const EdgeAutoScrollConfig &config);

    static QPointF clampToRect(const QPointF &pos, const QRectF &rect);

    // Integer scroll delta for one frame of dtMs milliseconds, carrying the
    // sub-pixel remainder across calls. Call resetAccumulator() when a scroll
    // session starts.
    QPoint computeStep(const QPointF &pointerPos, const QRectF &viewportRect, Qt::Orientations axes,
                       double dtMs);

    // computeStep variant carrying the drag press position (same semantics as
    // the pressPos-aware velocity overload above).
    QPoint computeStep(const QPointF &pointerPos, const QPointF &pressPos,
                       const QRectF &viewportRect, Qt::Orientations axes, double dtMs);
    void resetAccumulator();

    // --- Drag session ---

    void prepareDrag(const QPointF &pressPos, Qt::Orientations axes = {});
    void setDragAxes(Qt::Orientations axes);
    void updateDragState(const QPointF &pointerPos, const QRectF &viewportRect,
                         int startDragDistance);
    QPoint computeDragStep(const QPointF &pointerPos, const QRectF &viewportRect, double dtMs);
    void stopDrag();
    [[nodiscard]] bool isDragArmed() const;

    // --- Runner ---

    void start();
    void stop();
    [[nodiscard]] bool isRunning() const;

signals:
    // Emitted every timer tick with the measured frame delta time.
    void frame(double dtMs);

private:
    EdgeAutoScrollConfig m_config;
    QPointF m_accumulator;
    QTimer m_timer;
    QElapsedTimer m_elapsed;
    QPointF m_dragPressPos;
    Qt::Orientations m_dragAxes;
    bool m_dragArmed = false;
    bool m_dragDistanceReached = false;
};

#endif // EDGEAUTOSCROLLER_H
