#include "tst_editor_interaction.h"

#include <QtTest/QTest>
// Unit tests for the EdgeAutoScroller pure computation core:
//   - zero speed outside the hot zone, correct sign at each edge
//   - monotonic speed increase towards the edge, saturation outside the view
//   - corner produces two-axis velocity, disabled axes stay zero
//   - sub-pixel accumulation across injected frame times (timer jitter safe)
//   - pointer clamping stays inside the viewport rect

#include "UI/Views/Common/EdgeAutoScroller.h"

#include <QCoreApplication>
#include <QTextStream>

#include <cmath>

namespace {

    bool fuzzyEqual(const double a, const double b, const double eps = 1e-9) {
        return std::abs(a - b) < eps;
    }
}

void EditorInteractionTests::outsideHotZone() {
    const EdgeAutoScrollConfig cfg;
    const QRectF vp(0, 0, 800, 600);
    const auto bothAxes = Qt::Horizontal | Qt::Vertical;
    {
        const auto v = EdgeAutoScroller::velocity(QPointF(400, 300), vp, bothAxes, cfg);
        QVERIFY2((v.isNull()), "center of viewport must produce zero velocity");

        const auto vEdgeIn =
            EdgeAutoScroller::velocity(QPointF(cfg.hotZoneH, 300), vp, bothAxes, cfg);
        QVERIFY2((fuzzyEqual(vEdgeIn.x(), 0)), "inner hot zone boundary must produce zero speed");
    }
}

void EditorInteractionTests::edgeDirections() {
    const EdgeAutoScrollConfig cfg;
    const QRectF vp(0, 0, 800, 600);
    const auto bothAxes = Qt::Horizontal | Qt::Vertical;
    {
        const auto vLeft = EdgeAutoScroller::velocity(QPointF(10, 300), vp, bothAxes, cfg);
        QVERIFY2((vLeft.x() < 0 && fuzzyEqual(vLeft.y(), 0)), "left edge scrolls negative x only");

        const auto vRight = EdgeAutoScroller::velocity(QPointF(790, 300), vp, bothAxes, cfg);
        QVERIFY2((vRight.x() > 0 && fuzzyEqual(vRight.y(), 0)),
                 "right edge scrolls positive x only");

        const auto vTop = EdgeAutoScroller::velocity(QPointF(400, 10), vp, bothAxes, cfg);
        QVERIFY2((vTop.y() < 0 && fuzzyEqual(vTop.x(), 0)), "top edge scrolls negative y only");

        const auto vBottom = EdgeAutoScroller::velocity(QPointF(400, 590), vp, bothAxes, cfg);
        QVERIFY2((vBottom.y() > 0 && fuzzyEqual(vBottom.x(), 0)),
                 "bottom edge scrolls positive y only");
    }
}

void EditorInteractionTests::speedSaturation() {
    const EdgeAutoScrollConfig cfg;
    const QRectF vp(0, 0, 800, 600);
    const auto bothAxes = Qt::Horizontal | Qt::Vertical;
    {
        double prev = 0;
        bool monotonic = true;
        for (int x = cfg.hotZoneH; x >= 0; --x) {
            const auto v = EdgeAutoScroller::velocity(QPointF(x, 300), vp, bothAxes, cfg);
            const double speed = -v.x();
            if (speed < prev - 1e-9)
                monotonic = false;
            prev = speed;
        }
        QVERIFY2((monotonic), "speed must increase monotonically towards the left edge");

        const auto vAtEdge = EdgeAutoScroller::velocity(QPointF(0, 300), vp, bothAxes, cfg);
        QVERIFY2((fuzzyEqual(-vAtEdge.x(), cfg.maxSpeedH)), "speed at the edge equals maxSpeedH");

        const auto vOutside = EdgeAutoScroller::velocity(QPointF(-500, 300), vp, bothAxes, cfg);
        QVERIFY2((fuzzyEqual(-vOutside.x(), cfg.maxSpeedH)),
                 "speed outside the viewport saturates at maxSpeedH");

        const auto vOutsideY = EdgeAutoScroller::velocity(QPointF(400, 5000), vp, bothAxes, cfg);
        QVERIFY2((fuzzyEqual(vOutsideY.y(), cfg.maxSpeedV)),
                 "speed outside the viewport saturates at maxSpeedV");
    }
}

void EditorInteractionTests::cornerAndDisabledAxes() {
    const EdgeAutoScrollConfig cfg;
    const QRectF vp(0, 0, 800, 600);
    const auto bothAxes = Qt::Horizontal | Qt::Vertical;
    {
        const auto vCorner = EdgeAutoScroller::velocity(QPointF(5, 5), vp, bothAxes, cfg);
        QVERIFY2((vCorner.x() < 0 && vCorner.y() < 0), "top-left corner scrolls on both axes");

        const auto vHOnly = EdgeAutoScroller::velocity(QPointF(5, 5), vp, Qt::Horizontal, cfg);
        QVERIFY2((vHOnly.x() < 0 && fuzzyEqual(vHOnly.y(), 0)),
                 "vertical axis stays zero when only horizontal is enabled");

        const auto vVOnly = EdgeAutoScroller::velocity(QPointF(5, 5), vp, Qt::Vertical, cfg);
        QVERIFY2((fuzzyEqual(vVOnly.x(), 0) && vVOnly.y() < 0),
                 "horizontal axis stays zero when only vertical is enabled");

        const auto vNone = EdgeAutoScroller::velocity(QPointF(5, 5), vp, Qt::Orientations(), cfg);
        QVERIFY2((vNone.isNull()), "no axes enabled must produce zero velocity");
    }
}

void EditorInteractionTests::subpixelAccumulation() {
    const EdgeAutoScrollConfig cfg;
    const QRectF vp(0, 0, 800, 600);
    const auto bothAxes = Qt::Horizontal | Qt::Vertical;
    {
        EdgeAutoScroller scroller;
        scroller.resetAccumulator();

        // Pointer at the right edge: exactly maxSpeedH = 1200 px/s.
        // Feed jittery dt values summing to exactly 1 second.
        const double dts[] = {16, 17, 15, 33, 8, 16, 16, 12, 20, 16};
        double total = 0;
        long long scrolled = 0;
        double elapsed = 0;
        while (elapsed < 1000.0) {
            for (const double dt : dts) {
                if (elapsed >= 1000.0)
                    break;
                const double clampedDt = std::min(dt, 1000.0 - elapsed);
                const auto step = scroller.computeStep(QPointF(800, 300), vp, bothAxes, clampedDt);
                scrolled += step.x();
                QVERIFY2((step.y() == 0), "no vertical scroll at horizontal edge");
                elapsed += clampedDt;
                total += clampedDt;
            }
        }
        // After exactly 1s at 1200 px/s, accumulated distance must be within
        // one pixel of 1200 (sub-pixel remainder may hold back < 1 px).
        QVERIFY2((fuzzyEqual(total, 1000.0)), "test feeds exactly one second of frames");
        QVERIFY2((scrolled >= 1199 && scrolled <= 1200),
                 "1s at maxSpeedH accumulates 1200 px regardless of frame jitter");
    }
}

void EditorInteractionTests::fractionalRemainders() {
    const EdgeAutoScrollConfig cfg;
    const QRectF vp(0, 0, 800, 600);
    const auto bothAxes = Qt::Horizontal | Qt::Vertical;
    {
        EdgeAutoScroller scroller;
        scroller.resetAccumulator();

        // Just inside the hot zone: tiny speed. Individual 16ms steps yield
        // fractional pixels; over enough frames they must still add up.
        const QPointF pos(cfg.hotZoneH - 1, 300); // 1px into the zone
        long long scrolled = 0;
        for (int i = 0; i < 1000; ++i)
            scrolled += scroller.computeStep(pos, vp, bothAxes, 16).x();
        const auto v = EdgeAutoScroller::velocity(pos, vp, bothAxes, cfg);
        const auto expected = v.x() * 16.0;
        QVERIFY2((scrolled < 0), "tiny speeds still accumulate scroll over time");
        QVERIFY2((std::abs(scrolled - expected) <= 1),
                 "accumulated distance matches speed * time within one pixel");
    }
}

void EditorInteractionTests::pressDeadZone() {
    const EdgeAutoScrollConfig cfg;
    const QRectF vp(0, 0, 800, 600);
    const auto bothAxes = Qt::Horizontal | Qt::Vertical;
    {
        // Pressed in the top-left corner hot zones, moved toward the center:
        // no scroll on either axis, even though the pointer is still inside
        // the configured 72/56 px zones.
        const QPointF pressTL(20, 30);
        const auto vCenter =
            EdgeAutoScroller::velocity(QPointF(400, 300), pressTL, vp, bothAxes, cfg);
        QVERIFY2((vCenter.isNull()), "press in top-left zone, move to center: zero velocity");

        // Slightly right-down (the user's box-select gesture): still zero.
        const auto vInward =
            EdgeAutoScroller::velocity(QPointF(60, 50), pressTL, vp, bothAxes, cfg);
        QVERIFY2((vInward.isNull()),
                 "press in top-left zone, small move toward center: zero velocity");

        // Moving deeper toward the corner scrolls left and up.
        const auto vDeeper = EdgeAutoScroller::velocity(QPointF(5, 10), pressTL, vp, bothAxes, cfg);
        QVERIFY2((vDeeper.x() < 0 && vDeeper.y() < 0),
                 "moving beyond the press depth scrolls toward the edge");

        // Crossing over into the opposite hot zones scrolls in that direction.
        const auto vOpposite =
            EdgeAutoScroller::velocity(QPointF(790, 590), pressTL, vp, bothAxes, cfg);
        QVERIFY2((vOpposite.x() > 0 && vOpposite.y() > 0),
                 "crossing into the opposite zones scrolls outward");
    }
}

void EditorInteractionTests::pressAwareHorizontalAxis() {
    const EdgeAutoScrollConfig cfg;
    const QRectF vp(0, 0, 800, 600);
    const auto bothAxes = Qt::Horizontal | Qt::Vertical;
    {
        // Symmetry on all four corners: move to center stays zero, moving
        // deeper toward the pressed corner scrolls toward it.
        struct CornerCase {
            QPointF press;
            QPointF deeper;
        };

        const CornerCase cases[] = {
            {{10, 10},   {2, 2}    }, // top-left: scroll -x, -y
            {{790, 10},  {798, 2}  }, // top-right: scroll +x, -y
            {{10, 590},  {2, 598}  }, // bottom-left: scroll -x, +y
            {{790, 590}, {798, 598}}  // bottom-right: scroll +x, +y
        };
        const auto center = QPointF(400, 300);
        for (const auto &c : cases) {
            const auto vC = EdgeAutoScroller::velocity(center, c.press, vp, bothAxes, cfg);
            QVERIFY2((vC.isNull()), "press in a corner, move to center: zero velocity");

            const auto vD = EdgeAutoScroller::velocity(c.deeper, c.press, vp, bothAxes, cfg);
            const bool xSignOk = (c.deeper.x() > c.press.x()) ? vD.x() > 0 : vD.x() < 0;
            const bool ySignOk = (c.deeper.y() > c.press.y()) ? vD.y() > 0 : vD.y() < 0;
            QVERIFY2((vD.x() != 0 && xSignOk && vD.y() != 0 && ySignOk),
                     "moving deeper toward the pressed corner scrolls toward it");
        }
    }
}

void EditorInteractionTests::pressAwareDisabledAxes() {
    const EdgeAutoScrollConfig cfg;
    const QRectF vp(0, 0, 800, 600);
    const auto bothAxes = Qt::Horizontal | Qt::Vertical;
    {
        // Pressed exactly on the left edge: the narrowed zone must still let
        // the pointer scroll by moving further out.
        const QPointF pressEdge(0, 300);
        const auto vOut =
            EdgeAutoScroller::velocity(QPointF(-10, 300), pressEdge, vp, bothAxes, cfg);
        QVERIFY2((vOut.x() < 0 && fuzzyEqual(vOut.y(), 0)),
                 "press on the edge, move outward: scrolls toward the edge");

        const auto vIn =
            EdgeAutoScroller::velocity(QPointF(400, 300), pressEdge, vp, bothAxes, cfg);
        QVERIFY2((vIn.isNull()), "press on the edge, move to center: zero velocity");
    }
}

void EditorInteractionTests::pressOutsideHotZone() {
    const EdgeAutoScrollConfig cfg;
    const QRectF vp(0, 0, 800, 600);
    const auto bothAxes = Qt::Horizontal | Qt::Vertical;
    {
        const QPointF pressMid(400, 300);
        const auto vLeft =
            EdgeAutoScroller::velocity(QPointF(10, 300), pressMid, vp, bothAxes, cfg);
        QVERIFY2((vLeft.x() < 0 && fuzzyEqual(vLeft.y(), 0)),
                 "press outside the zones, enter the left zone: scrolls left");

        const auto vBottom =
            EdgeAutoScroller::velocity(QPointF(400, 590), pressMid, vp, bothAxes, cfg);
        QVERIFY2((vBottom.y() > 0 && fuzzyEqual(vBottom.x(), 0)),
                 "press outside the zones, enter the bottom zone: scrolls down");
    }
}

void EditorInteractionTests::pressAwareStep() {
    const EdgeAutoScrollConfig cfg;
    const QRectF vp(0, 0, 800, 600);
    const auto bothAxes = Qt::Horizontal | Qt::Vertical;
    {
        EdgeAutoScroller scroller;
        scroller.resetAccumulator();
        const QPointF pressTL(20, 30);
        const auto stepCenter = scroller.computeStep(QPointF(400, 300), pressTL, vp, bothAxes, 16);
        QVERIFY2((stepCenter.x() == 0 && stepCenter.y() == 0),
                 "computeStep with press position in the zone: no scroll toward center");

        const auto stepRight = scroller.computeStep(QPointF(790, 300), pressTL, vp, bothAxes, 16);
        QVERIFY2((stepRight.x() > 0 && stepRight.y() == 0),
                 "computeStep scrolls right after crossing into the right zone");
    }
}

void EditorInteractionTests::dragSessionLifecycle() {
    const EdgeAutoScrollConfig cfg;
    const QRectF vp(0, 0, 800, 600);
    const auto bothAxes = Qt::Horizontal | Qt::Vertical;
    {
        EdgeAutoScroller scroller;
        const QPointF press(400, 300);
        scroller.prepareDrag(press, bothAxes);
        QVERIFY2((scroller.isDragArmed() && !scroller.isRunning()),
                 "preparing a drag arms its axes without starting the timer");

        scroller.updateDragState(QPointF(405, 300), vp, 10);
        QVERIFY2((!scroller.isRunning()),
                 "movement below the drag threshold must not start scrolling");

        scroller.updateDragState(QPointF(790, 300), vp, 10);
        QVERIFY2((scroller.isRunning()),
                 "entering a hot zone after the threshold starts scrolling");
        const auto step = scroller.computeDragStep(QPointF(790, 300), vp, 16);
        QVERIFY2((step.x() > 0 && step.y() == 0),
                 "drag-session steps reuse the stored press position and axes");

        scroller.stopDrag();
        QVERIFY2((!scroller.isDragArmed() && !scroller.isRunning()),
                 "stopping a drag clears both session and runner state");
    }
}

void EditorInteractionTests::pointerClamping() {
    const EdgeAutoScrollConfig cfg;
    const QRectF vp(0, 0, 800, 600);
    const auto bothAxes = Qt::Horizontal | Qt::Vertical;
    {
        const auto c1 = EdgeAutoScroller::clampToRect(QPointF(-100, 300), vp);
        QVERIFY2((c1 == QPointF(0, 300)), "x clamps to the left edge");
        const auto c2 = EdgeAutoScroller::clampToRect(QPointF(900, -50), vp);
        QVERIFY2((c2 == QPointF(800, 0)), "clamps to the top-right corner");
        const auto c3 = EdgeAutoScroller::clampToRect(QPointF(400, 300), vp);
        QVERIFY2((c3 == QPointF(400, 300)), "inside position is unchanged");
        const auto c4 = EdgeAutoScroller::clampToRect(QPointF(5000, 5000), vp);
        QVERIFY2((vp.contains(c4)), "clamped position always inside the rect");
    }
}
