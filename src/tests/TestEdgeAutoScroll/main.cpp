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
    int g_failures = 0;

    bool expect(const bool condition, const char *message) {
        if (condition)
            return true;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        ++g_failures;
        return false;
    }

    bool fuzzyEqual(const double a, const double b, const double eps = 1e-9) {
        return std::abs(a - b) < eps;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    const EdgeAutoScrollConfig cfg; // defaults: 72/56 px zones, 1200/800 px/s
    const QRectF vp(0, 0, 800, 600);
    const auto bothAxes = Qt::Horizontal | Qt::Vertical;

    // --- Zero speed outside the hot zone ---
    {
        const auto v = EdgeAutoScroller::velocity(QPointF(400, 300), vp, bothAxes, cfg);
        expect(v.isNull(), "center of viewport must produce zero velocity");

        const auto vEdgeIn =
            EdgeAutoScroller::velocity(QPointF(cfg.hotZoneH, 300), vp, bothAxes, cfg);
        expect(fuzzyEqual(vEdgeIn.x(), 0), "inner hot zone boundary must produce zero speed");
    }

    // --- Direction correctness on all four edges ---
    {
        const auto vLeft = EdgeAutoScroller::velocity(QPointF(10, 300), vp, bothAxes, cfg);
        expect(vLeft.x() < 0 && fuzzyEqual(vLeft.y(), 0), "left edge scrolls negative x only");

        const auto vRight = EdgeAutoScroller::velocity(QPointF(790, 300), vp, bothAxes, cfg);
        expect(vRight.x() > 0 && fuzzyEqual(vRight.y(), 0), "right edge scrolls positive x only");

        const auto vTop = EdgeAutoScroller::velocity(QPointF(400, 10), vp, bothAxes, cfg);
        expect(vTop.y() < 0 && fuzzyEqual(vTop.x(), 0), "top edge scrolls negative y only");

        const auto vBottom = EdgeAutoScroller::velocity(QPointF(400, 590), vp, bothAxes, cfg);
        expect(vBottom.y() > 0 && fuzzyEqual(vBottom.x(), 0),
               "bottom edge scrolls positive y only");
    }

    // --- Monotonic increase towards the edge, saturation outside ---
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
        expect(monotonic, "speed must increase monotonically towards the left edge");

        const auto vAtEdge = EdgeAutoScroller::velocity(QPointF(0, 300), vp, bothAxes, cfg);
        expect(fuzzyEqual(-vAtEdge.x(), cfg.maxSpeedH), "speed at the edge equals maxSpeedH");

        const auto vOutside = EdgeAutoScroller::velocity(QPointF(-500, 300), vp, bothAxes, cfg);
        expect(fuzzyEqual(-vOutside.x(), cfg.maxSpeedH),
               "speed outside the viewport saturates at maxSpeedH");

        const auto vOutsideY = EdgeAutoScroller::velocity(QPointF(400, 5000), vp, bothAxes, cfg);
        expect(fuzzyEqual(vOutsideY.y(), cfg.maxSpeedV),
               "speed outside the viewport saturates at maxSpeedV");
    }

    // --- Corner produces two-axis velocity; disabled axes stay zero ---
    {
        const auto vCorner = EdgeAutoScroller::velocity(QPointF(5, 5), vp, bothAxes, cfg);
        expect(vCorner.x() < 0 && vCorner.y() < 0, "top-left corner scrolls on both axes");

        const auto vHOnly = EdgeAutoScroller::velocity(QPointF(5, 5), vp, Qt::Horizontal, cfg);
        expect(vHOnly.x() < 0 && fuzzyEqual(vHOnly.y(), 0),
               "vertical axis stays zero when only horizontal is enabled");

        const auto vVOnly = EdgeAutoScroller::velocity(QPointF(5, 5), vp, Qt::Vertical, cfg);
        expect(fuzzyEqual(vVOnly.x(), 0) && vVOnly.y() < 0,
               "horizontal axis stays zero when only vertical is enabled");

        const auto vNone = EdgeAutoScroller::velocity(QPointF(5, 5), vp, Qt::Orientations(), cfg);
        expect(vNone.isNull(), "no axes enabled must produce zero velocity");
    }

    // --- Sub-pixel accumulation with injected jittery frame times ---
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
                expect(step.y() == 0, "no vertical scroll at horizontal edge");
                elapsed += clampedDt;
                total += clampedDt;
            }
        }
        // After exactly 1s at 1200 px/s, accumulated distance must be within
        // one pixel of 1200 (sub-pixel remainder may hold back < 1 px).
        expect(fuzzyEqual(total, 1000.0), "test feeds exactly one second of frames");
        expect(scrolled >= 1199 && scrolled <= 1200,
               "1s at maxSpeedH accumulates 1200 px regardless of frame jitter");
    }

    // --- Accumulator carries remainders at very small speeds ---
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
        expect(scrolled < 0, "tiny speeds still accumulate scroll over time");
        expect(std::abs(scrolled - expected) <= 1,
               "accumulated distance matches speed * time within one pixel");
    }

    // --- Pointer clamping ---
    {
        const auto c1 = EdgeAutoScroller::clampToRect(QPointF(-100, 300), vp);
        expect(c1 == QPointF(0, 300), "x clamps to the left edge");
        const auto c2 = EdgeAutoScroller::clampToRect(QPointF(900, -50), vp);
        expect(c2 == QPointF(800, 0), "clamps to the top-right corner");
        const auto c3 = EdgeAutoScroller::clampToRect(QPointF(400, 300), vp);
        expect(c3 == QPointF(400, 300), "inside position is unchanged");
        const auto c4 = EdgeAutoScroller::clampToRect(QPointF(5000, 5000), vp);
        expect(vp.contains(c4), "clamped position always inside the rect");
    }

    if (g_failures == 0) {
        QTextStream(stdout) << "All EdgeAutoScroller tests passed" << Qt::endl;
        return 0;
    }
    QTextStream(stderr) << g_failures << " test(s) failed" << Qt::endl;
    return 1;
}
