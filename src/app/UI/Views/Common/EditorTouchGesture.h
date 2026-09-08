#ifndef EDITORTOUCHGESTURE_H
#define EDITORTOUCHGESTURE_H

#include <QList>
#include <QPointF>
#include <QVarLengthArray>

// Pure-logic touch gesture arbitration, deliberately free of any widget or Qt
// event dependency: callers feed it point ids, widget-local positions and a
// millisecond timestamp, and get back an ordered list of intents. Keeping the
// state machine headless is what makes it unit testable without a real
// touchscreen (see src/tests/TestTouchGestures).
//
// Recognized gestures:
//   - tap / double tap                 -> single stream, press + (dbl) + release
//   - drag past the tap slop           -> single stream, press + moves + release
//   - press held past the long press   -> LongPress, then the caller decides
//     timeout                             whether it becomes a menu or a drag
//   - two fingers                      -> navigation: pan by centroid delta and
//                                         per-axis pinch around the centroid
class EditorTouchGesture {
public:
    struct Config {
        // Movement below this radius keeps a press eligible for tap/long press.
        double tapSlopPx = 12.0;
        // A press that travels past this before the timeout can never long press.
        double longPressSlopPx = 12.0;
        qint64 longPressMs = 450;
        qint64 tapMaxMs = 400;
        qint64 doubleTapMaxMs = 350;
        double doubleTapSlopPx = 28.0;
        // Pinch is ignored while the two fingers are closer than this on an axis,
        // because the ratio of two tiny spans is mostly noise.
        double minPinchSpanPx = 32.0;
        // A dominant axis is locked in once its accumulated |ln(factor)| passes
        // axisLockThreshold and exceeds the other axis by axisLockRatio.
        double axisLockThreshold = 0.06;
        double axisLockRatio = 1.8;
        // Exponential smoothing weight kept on the previous velocity estimate.
        double velocitySmoothing = 0.55;
        // Flicks slower than this do not start an inertia glide.
        double inertiaMinVelocityPxPerSec = 90.0;
    };

    struct Event {
        enum class Type {
            SingleBegin,
            SingleMove,
            SingleEnd,
            // The single stream was aborted mid-flight (a second finger landed).
            SingleCancel,
            LongPress,
            NavigationBegin,
            NavigationUpdate,
            NavigationEnd,
        };

        Type type = Type::SingleBegin;
        // Widget-local position, valid for every single-stream event and for
        // LongPress (where it carries the original press position).
        QPointF position;
        // Navigation payload.
        QPointF panDelta;
        QPointF anchor;
        QPointF velocity; // px/s, only meaningful on NavigationEnd
        double horizontalFactor = 1.0;
        double verticalFactor = 1.0;
        // The single stream was promoted out of a long press, so the caller can
        // pick the "held" variant of an interaction (rubber band instead of a
        // direct manipulation, for instance).
        bool fromLongPress = false;
        bool doubleTap = false;
        // The stream is a tap, not a drag: press and release arrive together
        // and no motion will follow. Callers must not turn a tap into a
        // content-creating direct manipulation.
        bool tap = false;
    };

    using Events = QVarLengthArray<Event, 4>;

    enum class Phase {
        Idle,
        // First finger is down but the gesture is still undecided.
        Pending,
        // LongPress was reported and the caller has not answered yet.
        LongPressPending,
        // A synthetic press has been handed out and moves are flowing.
        Single,
        Navigation,
        // The gesture is over but fingers are still on the glass.
        Settling,
    };

    // The default argument cannot live here: a nested class's default member
    // initializers are only parsed once the enclosing class is complete.
    EditorTouchGesture();
    explicit EditorTouchGesture(const Config &config);

    void setConfig(const Config &config);
    [[nodiscard]] const Config &config() const;

    [[nodiscard]] Phase phase() const;
    [[nodiscard]] int activePointCount() const;
    // True while a synthetic pointer stream is being driven by this gesture.
    [[nodiscard]] bool hasSingleStream() const;
    [[nodiscard]] QPointF lastPosition() const;

    Events pressed(int id, const QPointF &position, qint64 timestampMs);
    Events moved(int id, const QPointF &position, qint64 timestampMs);
    Events released(int id, const QPointF &position, qint64 timestampMs);
    // Emit the accumulated two-finger delta. Call once per touch event, after
    // every point it carries has been fed in: computing pan and pinch from a
    // half-applied event would make one finger's motion look like a zoom.
    Events flushNavigation(qint64 timestampMs);
    // All points are gone and whatever was in flight must be undone.
    Events cancelled();

    // Long press timeout fired. Returns a LongPress event when the press is
    // still eligible, otherwise nothing.
    Events longPressTimeout(qint64 timestampMs);
    // Answer to LongPress: asDrag promotes it into a held single stream,
    // otherwise the stream is consumed (the caller opened a context menu).
    Events confirmLongPress(bool asDrag, qint64 timestampMs);

    // Deadline for the pending long press, or 0 when none is armed.
    [[nodiscard]] qint64 longPressDeadline() const;

private:
    struct Point {
        int id = -1;
        QPointF position;
    };

    [[nodiscard]] int indexOf(int id) const;
    void resetToIdle();
    void beginNavigation(qint64 timestampMs);
    Events updateNavigation(qint64 timestampMs);
    void updateAxisLock(double logX, double logY);

    Config m_config;
    Phase m_phase = Phase::Idle;
    QList<Point> m_points;

    // Single stream bookkeeping.
    int m_primaryId = -1;
    QPointF m_pressPosition;
    QPointF m_lastPosition;
    qint64 m_pressTimestamp = 0;
    bool m_fromLongPress = false;
    bool m_pendingDoubleTap = false;
    bool m_longPressEligible = false;

    // Double tap bookkeeping, carried across gestures.
    qint64 m_lastTapTimestamp = 0;
    QPointF m_lastTapPosition;

    // Navigation bookkeeping.
    int m_navigationIds[2] = {-1, -1};
    QPointF m_navigationCentroid;
    double m_navigationSpanX = 0.0;
    double m_navigationSpanY = 0.0;
    double m_navigationLogX = 0.0;
    double m_navigationLogY = 0.0;
    bool m_horizontalZoomLocked = false;
    bool m_verticalZoomLocked = false;
    bool m_axisLockDecided = false;
    QPointF m_navigationVelocity;
    qint64 m_navigationTimestamp = 0;
    bool m_navigationDirty = false;
};

#endif // EDITORTOUCHGESTURE_H
