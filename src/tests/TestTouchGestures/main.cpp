#include "UI/Views/Common/EditorTouchGesture.h"

#include <QTest>

#include <cmath>

using Gesture = EditorTouchGesture;
using Event = EditorTouchGesture::Event;
using Type = EditorTouchGesture::Event::Type;

namespace {
    // Index of the first event of the given type, or -1.
    int indexOf(const Gesture::Events &events, const Type type) {
        for (qsizetype i = 0; i < events.size(); ++i) {
            if (events.at(i).type == type)
                return static_cast<int>(i);
        }
        return -1;
    }

    bool contains(const Gesture::Events &events, const Type type) {
        return indexOf(events, type) >= 0;
    }
}

class TestTouchGestures final : public QObject {
    Q_OBJECT

private slots:
    // A short press that never travels becomes a click: press and release are
    // handed out together, flagged as a tap so the caller never mistakes it for
    // the beginning of a drag.
    void tapProducesPressAndRelease() {
        Gesture gesture;
        QVERIFY(gesture.pressed(1, {100, 100}, 0).isEmpty());
        const auto events = gesture.released(1, {102, 101}, 120);
        QCOMPARE(static_cast<int>(events.size()), 2);
        QCOMPARE(events.at(0).type, Type::SingleBegin);
        QVERIFY(events.at(0).tap);
        QVERIFY(!events.at(0).doubleTap);
        QCOMPARE(events.at(0).position, QPointF(100, 100));
        QCOMPARE(events.at(1).type, Type::SingleEnd);
        QCOMPARE(gesture.phase(), Gesture::Phase::Idle);
    }

    // A press that leaves the tap slop turns into a drag, and the synthetic
    // press is reported at the original press position so hit testing still
    // sees where the finger landed.
    void dragProducesPressAtTheOriginalPosition() {
        Gesture gesture;
        gesture.pressed(1, {100, 100}, 0);
        QVERIFY(gesture.moved(1, {104, 100}, 20).isEmpty()); // inside the slop
        const auto events = gesture.moved(1, {140, 100}, 40);
        QCOMPARE(static_cast<int>(events.size()), 2);
        QCOMPARE(events.at(0).type, Type::SingleBegin);
        QVERIFY(!events.at(0).tap);
        QCOMPARE(events.at(0).position, QPointF(100, 100));
        QCOMPARE(events.at(1).type, Type::SingleMove);
        QCOMPARE(events.at(1).position, QPointF(140, 100));

        const auto moved = gesture.moved(1, {160, 100}, 60);
        QCOMPARE(static_cast<int>(moved.size()), 1);
        QCOMPARE(moved.at(0).type, Type::SingleMove);

        const auto released = gesture.released(1, {160, 100}, 80);
        QCOMPARE(static_cast<int>(released.size()), 1);
        QCOMPARE(released.at(0).type, Type::SingleEnd);
    }

    void doubleTapIsReportedOnTheSecondTap() {
        Gesture gesture;
        gesture.pressed(1, {100, 100}, 0);
        auto events = gesture.released(1, {100, 100}, 80);
        QVERIFY(!events.at(0).doubleTap);

        gesture.pressed(2, {104, 102}, 200);
        events = gesture.released(2, {104, 102}, 260);
        QVERIFY(events.at(0).doubleTap);

        // A third tap starts a new chain rather than extending the double tap.
        gesture.pressed(3, {104, 102}, 400);
        events = gesture.released(3, {104, 102}, 450);
        QVERIFY(!events.at(0).doubleTap);
    }

    void doubleTapExpiresAfterTheTimeout() {
        Gesture gesture;
        gesture.pressed(1, {100, 100}, 0);
        gesture.released(1, {100, 100}, 80);
        gesture.pressed(2, {100, 100}, 2000);
        const auto events = gesture.released(2, {100, 100}, 2060);
        QVERIFY(!events.at(0).doubleTap);
    }

    // The long press timeout only reports the intent; the caller answers with
    // confirmLongPress() once it knows whether the finger landed on content.
    void longPressOnContentIsConsumed() {
        Gesture gesture;
        gesture.pressed(1, {100, 100}, 0);
        const auto deadline = gesture.longPressDeadline();
        QCOMPARE(deadline, gesture.config().longPressMs);

        const auto events = gesture.longPressTimeout(deadline);
        QCOMPARE(static_cast<int>(events.size()), 1);
        QCOMPARE(events.at(0).type, Type::LongPress);
        QCOMPARE(events.at(0).position, QPointF(100, 100));

        QVERIFY(gesture.confirmLongPress(false, deadline).isEmpty());
        // Nothing more comes out of that finger, not even on release.
        QVERIFY(gesture.moved(1, {200, 100}, deadline + 40).isEmpty());
        QVERIFY(gesture.released(1, {200, 100}, deadline + 60).isEmpty());
        QCOMPARE(gesture.phase(), Gesture::Phase::Idle);
    }

    void longPressOnBlankBecomesAHeldDrag() {
        Gesture gesture;
        gesture.pressed(1, {100, 100}, 0);
        const auto deadline = gesture.longPressDeadline();
        gesture.longPressTimeout(deadline);

        const auto events = gesture.confirmLongPress(true, deadline);
        QCOMPARE(static_cast<int>(events.size()), 1);
        QCOMPARE(events.at(0).type, Type::SingleBegin);
        QVERIFY(events.at(0).fromLongPress);
        QVERIFY(!events.at(0).tap);

        const auto moved = gesture.moved(1, {180, 140}, deadline + 40);
        QCOMPARE(static_cast<int>(moved.size()), 1);
        QCOMPARE(moved.at(0).type, Type::SingleMove);
        QVERIFY(moved.at(0).fromLongPress);
    }

    void movingPastTheSlopCancelsTheLongPress() {
        Gesture gesture;
        gesture.pressed(1, {100, 100}, 0);
        gesture.moved(1, {160, 100}, 50);
        QCOMPARE(gesture.longPressDeadline(), qint64(0));
        QVERIFY(gesture.longPressTimeout(500).isEmpty());
    }

    // The second finger always wins: whatever the first one had started is
    // cancelled, never committed.
    void secondFingerCancelsTheSingleStream() {
        Gesture gesture;
        gesture.pressed(1, {100, 100}, 0);
        gesture.moved(1, {150, 100}, 20);
        QCOMPARE(gesture.phase(), Gesture::Phase::Single);

        const auto events = gesture.pressed(2, {300, 100}, 40);
        QCOMPARE(static_cast<int>(events.size()), 2);
        QCOMPARE(events.at(0).type, Type::SingleCancel);
        QCOMPARE(events.at(1).type, Type::NavigationBegin);
        QCOMPARE(gesture.phase(), Gesture::Phase::Navigation);
    }

    void secondFingerBeforeAnyDragEmitsNoCancel() {
        Gesture gesture;
        gesture.pressed(1, {100, 100}, 0);
        const auto events = gesture.pressed(2, {300, 100}, 20);
        QCOMPARE(static_cast<int>(events.size()), 1);
        QCOMPARE(events.at(0).type, Type::NavigationBegin);
    }

    // Moving both fingers by the same amount is a pure pan: the centroid
    // travels and neither span changes, so no zoom leaks in.
    void twoFingerPanCarriesNoZoom() {
        Gesture gesture;
        gesture.pressed(1, {100, 100}, 0);
        gesture.pressed(2, {300, 100}, 0);

        gesture.moved(1, {150, 130}, 16);
        gesture.moved(2, {350, 130}, 16);
        const auto events = gesture.flushNavigation(16);
        QCOMPARE(static_cast<int>(events.size()), 1);
        QCOMPARE(events.at(0).type, Type::NavigationUpdate);
        QCOMPARE(events.at(0).panDelta, QPointF(50, 30));
        QCOMPARE(events.at(0).horizontalFactor, 1.0);
        QCOMPARE(events.at(0).verticalFactor, 1.0);
    }

    // Feeding the points one at a time must not look like a pinch. This is the
    // reason navigation is flushed once per touch event instead of per point.
    void partiallyAppliedTouchEventEmitsNothing() {
        Gesture gesture;
        gesture.pressed(1, {100, 100}, 0);
        gesture.pressed(2, {300, 100}, 0);
        QVERIFY(gesture.moved(1, {150, 100}, 16).isEmpty());
        QVERIFY(gesture.moved(2, {350, 100}, 16).isEmpty());
        const auto events = gesture.flushNavigation(16);
        QCOMPARE(static_cast<int>(events.size()), 1);
        QCOMPARE(events.at(0).horizontalFactor, 1.0);
    }

    // Spreading horizontally scales the time axis and leaves the key axis alone.
    void horizontalPinchLocksTheHorizontalAxis() {
        Gesture gesture;
        gesture.pressed(1, {100, 300}, 0);
        gesture.pressed(2, {300, 300}, 0);

        gesture.moved(1, {50, 300}, 16);
        gesture.moved(2, {350, 300}, 16);
        const auto events = gesture.flushNavigation(16);
        QCOMPARE(static_cast<int>(events.size()), 1);
        QVERIFY(std::abs(events.at(0).horizontalFactor - 1.5) < 1e-9);
        QCOMPARE(events.at(0).verticalFactor, 1.0);
        QCOMPARE(events.at(0).anchor, QPointF(200, 300));
    }

    void verticalPinchLocksTheVerticalAxis() {
        Gesture gesture;
        gesture.pressed(1, {300, 100}, 0);
        gesture.pressed(2, {300, 300}, 0);

        gesture.moved(1, {300, 50}, 16);
        gesture.moved(2, {300, 350}, 16);
        const auto events = gesture.flushNavigation(16);
        QCOMPARE(static_cast<int>(events.size()), 1);
        QCOMPARE(events.at(0).horizontalFactor, 1.0);
        QVERIFY(std::abs(events.at(0).verticalFactor - 1.5) < 1e-9);
    }

    // A diagonal spread that grows both spans comparably keeps both axes live.
    void diagonalPinchKeepsBothAxes() {
        Gesture gesture;
        gesture.pressed(1, {100, 100}, 0);
        gesture.pressed(2, {300, 300}, 0);

        gesture.moved(1, {50, 50}, 16);
        gesture.moved(2, {350, 350}, 16);
        const auto events = gesture.flushNavigation(16);
        QCOMPARE(static_cast<int>(events.size()), 1);
        QVERIFY(std::abs(events.at(0).horizontalFactor - 1.5) < 1e-9);
        QVERIFY(std::abs(events.at(0).verticalFactor - 1.5) < 1e-9);
    }

    // Once an axis is locked it stays locked for the rest of the gesture, so a
    // sloppy finger later on cannot start scaling the other axis.
    void theAxisLockHoldsForTheWholeGesture() {
        Gesture gesture;
        // A vertical span well past minPinchSpanPx from the start, so the
        // later vertical spread is rejected by the lock and not by the noise
        // gate.
        gesture.pressed(1, {100, 250}, 0);
        gesture.pressed(2, {300, 350}, 0);

        gesture.moved(1, {50, 250}, 16);
        gesture.moved(2, {350, 350}, 16);
        auto events = gesture.flushNavigation(16);
        QCOMPARE(static_cast<int>(events.size()), 1);
        QVERIFY(std::abs(events.at(0).horizontalFactor - 1.5) < 1e-9);
        QCOMPARE(events.at(0).verticalFactor, 1.0);

        // Now spread vertically by 3x: the horizontal lock must swallow it.
        gesture.moved(1, {50, 150}, 32);
        gesture.moved(2, {350, 450}, 32);
        events = gesture.flushNavigation(32);
        QCOMPARE(static_cast<int>(events.size()), 1);
        QCOMPARE(events.at(0).verticalFactor, 1.0);
    }

    // A flick reports the smoothed centroid velocity so the caller can glide.
    void navigationEndReportsFlickVelocity() {
        Gesture gesture;
        gesture.pressed(1, {100, 100}, 0);
        gesture.pressed(2, {300, 100}, 0);
        for (int step = 1; step <= 6; ++step) {
            const auto x = 40.0 * step;
            gesture.moved(1, {100 + x, 100}, step * 16);
            gesture.moved(2, {300 + x, 100}, step * 16);
            gesture.flushNavigation(step * 16);
        }
        const auto events = gesture.released(1, {340, 100}, 6 * 16);
        QCOMPARE(static_cast<int>(events.size()), 1);
        QCOMPARE(events.at(0).type, Type::NavigationEnd);
        QVERIFY(events.at(0).velocity.x() > 1000.0);
        QCOMPARE(events.at(0).velocity.y(), 0.0);
    }

    void slowReleaseReportsNoInertia() {
        Gesture gesture;
        gesture.pressed(1, {100, 100}, 0);
        gesture.pressed(2, {300, 100}, 0);
        for (int step = 1; step <= 6; ++step) {
            const auto x = static_cast<double>(step);
            gesture.moved(1, {100 + x, 100}, step * 100);
            gesture.moved(2, {300 + x, 100}, step * 100);
            gesture.flushNavigation(step * 100);
        }
        const auto events = gesture.released(1, {106, 100}, 600);
        QCOMPARE(events.at(0).type, Type::NavigationEnd);
        QCOMPARE(events.at(0).velocity, QPointF(0, 0));
    }

    // Lifting one of the two fingers ends navigation for good: the remaining
    // finger must not silently turn into an edit.
    void theRemainingFingerDoesNotStartAnEdit() {
        Gesture gesture;
        gesture.pressed(1, {100, 100}, 0);
        gesture.pressed(2, {300, 100}, 0);
        gesture.released(1, {100, 100}, 40);
        QCOMPARE(gesture.phase(), Gesture::Phase::Settling);
        QVERIFY(gesture.moved(2, {400, 200}, 60).isEmpty());
        QVERIFY(gesture.released(2, {400, 200}, 80).isEmpty());
        QCOMPARE(gesture.phase(), Gesture::Phase::Idle);
    }

    // A long press that the caller consumed (it opened a context menu) leaves
    // the finger resting on the glass. A second finger must still be able to
    // start navigation, otherwise two-finger zoom is dead until the whole hand
    // is lifted.
    void aConsumedLongPressDoesNotBlockNavigation() {
        Gesture gesture;
        gesture.pressed(1, {100, 100}, 0);
        const auto deadline = gesture.longPressDeadline();
        gesture.longPressTimeout(deadline);
        gesture.confirmLongPress(false, deadline);
        QCOMPARE(gesture.phase(), Gesture::Phase::Settling);

        const auto events = gesture.pressed(2, {300, 100}, deadline + 200);
        QCOMPARE(static_cast<int>(events.size()), 1);
        QCOMPARE(events.at(0).type, Type::NavigationBegin);
        QCOMPARE(gesture.phase(), Gesture::Phase::Navigation);

        gesture.moved(1, {50, 100}, deadline + 216);
        gesture.moved(2, {350, 100}, deadline + 216);
        const auto update = gesture.flushNavigation(deadline + 216);
        QCOMPARE(static_cast<int>(update.size()), 1);
        QVERIFY(std::abs(update.at(0).horizontalFactor - 1.5) < 1e-9);
    }

    // Putting a finger back down after a two-finger gesture resumes navigation
    // rather than waiting for the leftover finger to be lifted first.
    void aFingerReturningAfterNavigationResumesNavigation() {
        Gesture gesture;
        gesture.pressed(1, {100, 100}, 0);
        gesture.pressed(2, {300, 100}, 0);
        gesture.released(1, {100, 100}, 40);
        QCOMPARE(gesture.phase(), Gesture::Phase::Settling);

        const auto events = gesture.pressed(3, {100, 100}, 80);
        QCOMPARE(static_cast<int>(events.size()), 1);
        QCOMPARE(events.at(0).type, Type::NavigationBegin);
    }

    // A single leftover finger still must not start an edit on its own.
    void aSingleFingerCannotRestartFromSettling() {
        Gesture gesture;
        gesture.pressed(1, {100, 100}, 0);
        gesture.pressed(2, {300, 100}, 0);
        gesture.released(1, {100, 100}, 40);
        gesture.released(2, {300, 100}, 60);
        QCOMPARE(gesture.phase(), Gesture::Phase::Idle);

        // Same sequence but with the leftover finger still down.
        Gesture other;
        other.pressed(1, {100, 100}, 0);
        other.pressed(2, {300, 100}, 0);
        other.released(1, {100, 100}, 40);
        QVERIFY(other.moved(2, {400, 100}, 60).isEmpty());
        QCOMPARE(other.phase(), Gesture::Phase::Settling);
    }

    // A release Qt never delivered would otherwise wedge the machine forever.
    void syncingActivePointsRecoversFromALostRelease() {
        Gesture gesture;
        gesture.pressed(1, {100, 100}, 0);
        gesture.pressed(2, {300, 100}, 0);
        QCOMPARE(gesture.phase(), Gesture::Phase::Navigation);

        // The platform reports an empty set without ever sending the releases.
        const auto events = gesture.syncActivePoints({});
        QVERIFY(contains(events, Type::NavigationEnd));
        QCOMPARE(gesture.phase(), Gesture::Phase::Idle);
        QCOMPARE(gesture.activePointCount(), 0);

        // And the next gesture works normally.
        gesture.pressed(3, {100, 100}, 200);
        QCOMPARE(gesture.phase(), Gesture::Phase::Pending);
    }

    void syncingActivePointsCancelsALostSingleStream() {
        Gesture gesture;
        gesture.pressed(1, {100, 100}, 0);
        gesture.moved(1, {200, 100}, 20);
        const auto events = gesture.syncActivePoints({});
        QVERIFY(contains(events, Type::SingleCancel));
        QCOMPARE(gesture.phase(), Gesture::Phase::Idle);
    }

    void syncingActivePointsKeepsPointsThatAreStillDown() {
        Gesture gesture;
        gesture.pressed(1, {100, 100}, 0);
        gesture.pressed(2, {300, 100}, 0);
        QVERIFY(gesture.syncActivePoints({1, 2}).isEmpty());
        QCOMPARE(gesture.phase(), Gesture::Phase::Navigation);
        QCOMPARE(gesture.activePointCount(), 2);
    }

    // A stray third contact, a resting thumb for instance, joins the gesture
    // without disturbing it. When one of the original pair then lifts, the two
    // fingers still on the glass have to keep navigating: leaving them in
    // Settling makes two-finger pan and zoom freeze until the whole hand comes
    // off, which is the shape of the "two fingers get stuck" report.
    void liftingOneOfThreeFingersHandsNavigationToTheRest() {
        Gesture gesture;
        gesture.pressed(1, {100, 100}, 0);
        gesture.pressed(2, {300, 100}, 0);
        gesture.pressed(3, {500, 100}, 20);
        QCOMPARE(gesture.phase(), Gesture::Phase::Navigation);
        QCOMPARE(gesture.activePointCount(), 3);

        const auto events = gesture.released(1, {100, 100}, 40);
        QVERIFY(contains(events, Type::NavigationEnd));
        QVERIFY(contains(events, Type::NavigationBegin));
        QCOMPARE(gesture.phase(), Gesture::Phase::Navigation);

        // The hand over must not fling the viewport on the way.
        QCOMPARE(events.at(indexOf(events, Type::NavigationEnd)).velocity, QPointF(0, 0));

        gesture.moved(2, {280, 100}, 56);
        gesture.moved(3, {520, 100}, 56);
        const auto update = gesture.flushNavigation(56);
        QCOMPARE(static_cast<int>(update.size()), 1);
        QCOMPARE(update.at(0).type, Type::NavigationUpdate);
    }

    // Losing only one of the two navigation fingers still ends the gesture,
    // because a single finger is not navigation.
    void liftingOneOfTwoFingersEndsNavigation() {
        Gesture gesture;
        gesture.pressed(1, {100, 100}, 0);
        gesture.pressed(2, {300, 100}, 0);
        const auto events = gesture.released(1, {100, 100}, 40);
        QVERIFY(contains(events, Type::NavigationEnd));
        QVERIFY(!contains(events, Type::NavigationBegin));
        QCOMPARE(gesture.phase(), Gesture::Phase::Settling);
    }

    // Fingers that are still on the glass after the machine lost its state (a
    // touch cancel, a pointer capture change) must be picked up again from
    // their next move. Ignoring unknown ids leaves them dead until the hand is
    // lifted, which is how a cancelled pinch used to freeze the viewport.
    void anUnknownFingerIsAdoptedInsteadOfIgnored() {
        Gesture gesture;
        gesture.pressed(1, {100, 100}, 0);
        gesture.pressed(2, {300, 100}, 0);
        gesture.cancelled();
        QCOMPARE(gesture.phase(), Gesture::Phase::Idle);
        QVERIFY(!gesture.tracksPoint(1));

        gesture.pressed(1, {110, 100}, 40, true);
        const auto events = gesture.pressed(2, {290, 100}, 40, true);
        QVERIFY(contains(events, Type::NavigationBegin));
        QCOMPARE(gesture.phase(), Gesture::Phase::Navigation);

        gesture.moved(1, {120, 100}, 56);
        gesture.moved(2, {280, 100}, 56);
        QCOMPARE(static_cast<int>(gesture.flushNavigation(56).size()), 1);
    }

    // An adopted finger has been down for an unknown time, so it must not arm
    // the long press: the menu would pop under a finger that is only resting.
    void anAdoptedFingerNeverArmsTheLongPress() {
        Gesture gesture;
        gesture.pressed(1, {100, 100}, 0, true);
        QCOMPARE(gesture.phase(), Gesture::Phase::Pending);
        QCOMPARE(gesture.longPressDeadline(), qint64(0));
        QVERIFY(gesture.longPressTimeout(1000).isEmpty());
    }

    void cancelUndoesAnInFlightDrag() {
        Gesture gesture;
        gesture.pressed(1, {100, 100}, 0);
        gesture.moved(1, {200, 100}, 20);
        const auto events = gesture.cancelled();
        QVERIFY(contains(events, Type::SingleCancel));
        QCOMPARE(gesture.phase(), Gesture::Phase::Idle);
        QCOMPARE(gesture.activePointCount(), 0);
    }

    void cancelEndsNavigation() {
        Gesture gesture;
        gesture.pressed(1, {100, 100}, 0);
        gesture.pressed(2, {300, 100}, 0);
        const auto events = gesture.cancelled();
        QVERIFY(contains(events, Type::NavigationEnd));
        QCOMPARE(events.at(indexOf(events, Type::NavigationEnd)).velocity, QPointF(0, 0));
        QCOMPARE(gesture.phase(), Gesture::Phase::Idle);
    }

    // A press held past the tap timeout that is never confirmed as a long press
    // (the caller did not run the timer) must not fire a click on release.
    void slowReleaseWithoutMotionIsNotATap() {
        Gesture gesture;
        gesture.pressed(1, {100, 100}, 0);
        QVERIFY(gesture.released(1, {100, 100}, 2000).isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestTouchGestures)

#include "main.moc"
