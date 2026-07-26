#include <lite/MusicBase/MusicTime.h>
#include <lite/MusicBase/MusicTimeConverter.h>
#include <lite/MusicBase/Timeline.h>
#include <lite/MusicBase/TimelineSnapUtils.h>

#include <QCoreApplication>

#include <cmath>
#include <cstdio>

namespace {
    bool expect(const bool condition, const char *message) {
        if (condition)
            return true;
        std::fprintf(stderr, "FAILED: %s\n", message);
        return false;
    }

    bool expectNear(const double actual, const double expected, const char *message,
                    const double epsilon = 1e-6) {
        if (std::abs(actual - expected) <= epsilon)
            return true;
        std::fprintf(stderr, "FAILED: %s (actual %.12f, expected %.12f)\n", message, actual,
                     expected);
        return false;
    }

    // A single-point timeline must behave exactly like the legacy global
    // converter, bit for bit.
    bool testSinglePointMatchesLegacyConverter() {
        bool ok = true;
        const double tempi[] = {120.0, 127.3, 60.0, 240.0, 33.34};
        const double ticks[] = {0.0, 1.0, 5.0, 479.0, 480.0, 481.0, 960.5, 12345.678, -960.0};
        for (const auto tempo : tempi) {
            const Timeline timeline({
                {0, tempo}
            });
            for (const auto tick : ticks) {
                const auto viaTimeline = timeline.tickToMs(tick);
                const auto viaLegacy = MusicTimeConverter::tickToMs(tick, tempo);
                ok &= expect(viaTimeline == viaLegacy,
                             "single-point tickToMs is bit-identical to legacy converter");
                const auto backViaTimeline = timeline.msToTick(viaTimeline);
                const auto backViaLegacy = MusicTimeConverter::msToTick(viaLegacy, tempo);
                ok &= expect(backViaTimeline == backViaLegacy,
                             "single-point msToTick is bit-identical to legacy converter");
                ok &= expect(timeline.tickToSec(tick) == MusicTimeConverter::tickToSec(tick, tempo),
                             "single-point tickToSec is bit-identical to legacy converter");
            }
        }
        // Legacy formatted display for a 4/4 project.
        const Timeline timeline({
            {0, 120.0}
        });
        for (const int tick : {0, 1, 479, 480, 1919, 1920, 1921, 76800}) {
            ok &= expect(timeline.getBarBeatTickTime(tick) ==
                             MusicTimeConverter::getBarBeatTickTime(tick, 4, 4),
                         "single-point getBarBeatTickTime matches legacy converter");
        }
        return ok;
    }

    // Degenerate equivalence: several same-valued points must behave exactly
    // like a single point.
    bool testDegenerateEquivalence() {
        bool ok = true;
        const Timeline single({
            {0, 127.3}
        });
        const Timeline multi(
            {
                {0,     127.3},
                {9600,  127.3},
                {19200, 127.3}
        },
            {TimeSignature(0, 4, 4), TimeSignature(4, 4, 4), TimeSignature(12, 4, 4)});
        for (const double tick : {0.0, 1.5, 9599.0, 9600.0, 9601.0, 19199.5, 19200.0, 19201.0,
                                  50000.25, -100.0}) {
            ok &= expectNear(multi.tickToMs(tick), single.tickToMs(tick),
                             "degenerate tickToMs equals single point");
            const auto ms = single.tickToMs(tick);
            ok &= expectNear(multi.msToTick(ms), single.msToTick(ms),
                             "degenerate msToTick equals single point");
        }
        for (const int tick : {0, 1, 7679, 7680, 7681, 23039, 23040, 23041, 76800}) {
            ok &= expect(multi.tickToTime(tick) == single.tickToTime(tick),
                         "degenerate tickToTime equals single point");
            ok &= expect(multi.getBarBeatTickTime(tick) == single.getBarBeatTickTime(tick),
                         "degenerate getBarBeatTickTime equals single point");
        }
        for (int bar = 0; bar < 32; bar++) {
            ok &= expect(multi.barToTick(bar) == single.barToTick(bar),
                         "degenerate barToTick equals single point");
        }
        return ok;
    }

    bool testTickMsRoundTrip() {
        bool ok = true;
        const Timeline timeline({
            {0,     120.0},
            {1920,  60.0 },
            {3840,  180.5},
            {19200, 240.0}
        });
        // Segment interiors, boundaries and boundaries +/- 1.
        const double ticks[] = {0.0,    1.0,    1919.0, 1920.0,  1921.0,  3839.0, 3840.0,
                                3841.0, 5000.5, 19199,  19200.0, 19201.0, 1e6};
        double previousMs = -1e18;
        for (const auto tick : ticks) {
            const auto ms = timeline.tickToMs(tick);
            ok &= expect(ms > previousMs, "tickToMs is strictly increasing");
            previousMs = ms;
            ok &= expectNear(timeline.msToTick(ms), tick, "tick -> ms -> tick round trip", 1e-6);
        }
        // ms -> tick -> ms round trip on a uniform sweep.
        for (double ms = 0.0; ms <= 60000.0; ms += 313.7) {
            ok &= expectNear(timeline.tickToMs(timeline.msToTick(ms)), ms,
                             "ms -> tick -> ms round trip", 1e-6);
        }
        // Second-domain helpers are consistent with the ms-domain ones.
        ok &= expectNear(timeline.tickToSec(3840.0), timeline.tickToMs(3840.0) / 1000.0,
                         "tickToSec consistent with tickToMs");
        ok &= expectNear(timeline.secToTick(2.5), timeline.msToTick(2500.0),
                         "secToTick consistent with msToTick");
        return ok;
    }

    bool testTempoQueries() {
        bool ok = true;
        const Timeline timeline({
            {0,    120.0},
            {1920, 60.0 },
            {3840, 180.0}
        });
        ok &= expectNear(timeline.tempoAt(0), 120.0, "tempoAt at tick 0");
        ok &= expectNear(timeline.tempoAt(1919), 120.0, "tempoAt just before a point");
        ok &= expectNear(timeline.tempoAt(1920), 60.0, "tempoAt exactly on a point");
        ok &= expectNear(timeline.tempoAt(99999), 180.0, "tempoAt after the last point");
        ok &= expect(timeline.nearestTickWithTempoTo(0) == 0, "nearest tempo tick at 0");
        ok &= expect(timeline.nearestTickWithTempoTo(1919) == 0, "nearest tempo tick before point");
        ok &= expect(timeline.nearestTickWithTempoTo(1920) == 1920, "nearest tempo tick on point");
        ok &= expect(timeline.nearestTickWithTempoTo(5000) == 3840, "nearest tempo tick in tail");
        return ok;
    }

    // 4/4 for bars 0..3, 3/4 for bars 4..7, 6/8 from bar 8 on.
    Timeline threeMeterTimeline() {
        return Timeline({
                            {0, 120.0}
        },
                        {TimeSignature(0, 4, 4), TimeSignature(4, 3, 4), TimeSignature(8, 6, 8)});
    }

    bool testBarTickMapping() {
        bool ok = true;
        const auto timeline = threeMeterTimeline();
        // 4/4 bars are 1920 ticks, 3/4 bars 1440 ticks, 6/8 bars 6 * 240 = 1440 ticks.
        ok &= expect(timeline.barToTick(0) == 0, "bar 0 starts at tick 0");
        ok &= expect(timeline.barToTick(1) == 1920, "bar 1 in 4/4");
        ok &= expect(timeline.barToTick(4) == 7680, "meter change bar 4");
        ok &= expect(timeline.barToTick(5) == 7680 + 1440, "bar 5 in 3/4");
        ok &= expect(timeline.barToTick(8) == 7680 + 4 * 1440, "meter change bar 8");
        ok &= expect(timeline.barToTick(9) == 13440 + 1440, "bar 9 in 6/8");
        ok &= expect(timeline.barToTick(-1) == -1, "negative bar yields -1");

        ok &= expect(timeline.timeSignatureAt(0).numerator == 4, "signature at bar 0");
        ok &= expect(timeline.timeSignatureAt(3).numerator == 4, "signature at bar 3");
        ok &= expect(timeline.timeSignatureAt(4).numerator == 3, "signature at bar 4");
        ok &= expect(timeline.timeSignatureAt(7).numerator == 3, "signature at bar 7");
        ok &= expect(timeline.timeSignatureAt(8).denominator == 8, "signature at bar 8");
        ok &= expect(timeline.timeSignatureAt(100).denominator == 8, "signature in tail");
        ok &= expect(timeline.nearestBarWithTimeSignatureTo(7) == 4, "nearest signature bar");
        ok &= expect(timeline.nearestBarWithTimeSignatureTo(8) == 8, "nearest signature on point");
        return ok;
    }

    bool testTickTimeRoundTrip() {
        bool ok = true;
        const auto timeline = threeMeterTimeline();
        // Round trip across all three segments including boundaries +/- 1.
        for (const int tick : {0, 1, 1919, 1920, 7679, 7680, 7681, 9119, 9120, 13439, 13440, 13441,
                               13680, 14879, 14880, 99999}) {
            const auto time = timeline.tickToTime(tick);
            ok &= expect(time.isValid(), "tickToTime yields a valid triple");
            ok &= expect(timeline.timeToTick(time) == tick, "tick -> time -> tick round trip");
        }
        // Spot checks for measure accumulation across meter changes.
        ok &= expect(timeline.tickToTime(7680) == MusicTime(4, 0, 0), "first 3/4 bar");
        ok &= expect(timeline.tickToTime(13440) == MusicTime(8, 0, 0), "first 6/8 bar");
        ok &= expect(timeline.tickToTime(14000) == MusicTime(8, 2, 80), "6/8 interior position");
        // barToTick(bar) == timeToTick(bar, 0, 0) for every bar.
        for (int bar = 0; bar < 64; bar++) {
            ok &= expect(timeline.barToTick(bar) == timeline.timeToTick(bar, 0, 0),
                         "barToTick equals timeToTick(bar, 0, 0)");
        }
        // Beat overflow spills forward using the measure's own beat length.
        ok &= expect(timeline.timeToTick(0, 4, 0) == 1920, "beat overflow spills forward");
        // Invalid inputs return -1 instead of crashing.
        ok &= expect(timeline.timeToTick(-1, 0, 0) == -1, "negative measure yields -1");
        ok &= expect(timeline.timeToTick(0, -1, 0) == -1, "negative beat yields -1");
        ok &= expect(timeline.timeToTick(0, 0, -1) == -1, "negative tick yields -1");
        ok &= expect(!timeline.tickToTime(-5).isValid(), "negative tick yields invalid time");
        return ok;
    }

    bool testZeroPointInvariant() {
        bool ok = true;
        Timeline timeline({
            {0,    120.0},
            {1920, 60.0 }
        },
                          {TimeSignature(0, 4, 4), TimeSignature(4, 3, 4)});
        ok &= expect(!timeline.removeTempoAt(0), "tempo point at tick 0 is not removable");
        ok &= expect(!timeline.removeTimeSignatureAt(0), "signature at bar 0 is not removable");
        ok &= expect(timeline.tempos().size() == 2, "tempo list intact after refused removal");
        ok &= expect(timeline.timeSignatures().size() == 2,
                     "signature list intact after refused removal");
        ok &= expectNear(timeline.tempoAt(0), 120.0, "conversions intact after refused removal");

        ok &= expect(timeline.removeTempoAt(1920), "removing an existing tempo point succeeds");
        ok &= expect(!timeline.removeTempoAt(1920), "removing a missing tempo point fails");
        ok &= expect(timeline.removeTimeSignatureAt(4), "removing an existing signature succeeds");
        ok &= expect(timeline.tempos().size() == 1 && timeline.timeSignatures().size() == 1,
                     "lists shrink after successful removal");

        // A timeline built without a zero point anchors the earliest point at 0.
        const Timeline anchored({
            {960, 90.0}
        });
        ok &= expect(anchored.tempos().size() == 2 && anchored.tempos().first().pos == 0,
                     "missing zero tempo point is anchored");
        ok &= expectNear(anchored.tempoAt(0), 90.0, "anchored point copies the earliest value");
        const Timeline empty(QList<Tempo>{}, QList<TimeSignature>{});
        ok &= expect(empty.tempos().size() == 1 && empty.tempos().first().pos == 0,
                     "empty tempo list falls back to the default point");
        ok &= expect(empty.timeSignatures().size() == 1 &&
                         empty.timeSignatures().first().barIndex == 0,
                     "empty signature list falls back to the default point");
        return ok;
    }

    bool testMutationApi() {
        bool ok = true;
        Timeline timeline;
        timeline.addTempo({1920, 60.0});
        timeline.addTempo({960, 90.0});
        ok &= expect(timeline.tempos().size() == 3, "addTempo inserts sorted");
        ok &= expect(timeline.tempos()[1].pos == 960, "addTempo keeps order");
        timeline.addTempo({960, 95.0});
        ok &= expect(timeline.tempos().size() == 3, "addTempo replaces at existing pos");
        ok &= expectNear(timeline.tempoAt(960), 95.0, "replaced tempo value is effective");

        timeline.addTimeSignature(TimeSignature(4, 3, 4));
        timeline.addTimeSignature(TimeSignature(2, 6, 8));
        ok &= expect(timeline.timeSignatures().size() == 3, "addTimeSignature inserts sorted");
        ok &= expect(timeline.timeSignatures()[1].barIndex == 2, "addTimeSignature keeps order");
        ok &= expect(timeline.barToTick(3) == 2 * 1920 + 1440, "grid follows inserted signature");

        // Duplicate positions in bulk input keep the first occurrence.
        Timeline duplicated({
            {0,   120.0},
            {960, 140.0},
            {960, 150.0}
        });
        ok &= expect(duplicated.tempos().size() == 2, "duplicate tempo positions are dropped");
        ok &= expectNear(duplicated.tempoAt(960), 140.0, "first duplicate wins");
        return ok;
    }

    bool testExtremeValues() {
        bool ok = true;
        // Extremely slow and fast tempi keep the round trip stable.
        const Timeline extremes({
            {0,    1.0   },
            {1920, 999.0 },
            {3840, 20.5  },
            {5760, 500.25}
        });
        for (const double tick : {0.0, 1919.0, 1920.0, 3839.5, 3840.0, 5760.0, 100000.0}) {
            ok &= expectNear(extremes.msToTick(extremes.tickToMs(tick)), tick,
                             "extreme tempo round trip", 1e-6);
        }
        // All power-of-two denominators up to 128 (and denominator 1).
        for (const int denominator : {1, 2, 4, 8, 16, 32, 64, 128}) {
            const Timeline timeline({
                                        {0, 120.0}
            },
                                    {TimeSignature(0, 3, denominator)});
            const int barTicks = 3 * (MusicTime::ticksPerWholeNote / denominator);
            ok &= expect(timeline.barToTick(5) == 5 * barTicks, "barToTick for denominator");
            for (const int tick : {0, barTicks - 1, barTicks, barTicks + 1, 10 * barTicks + 7}) {
                ok &= expect(timeline.timeToTick(timeline.tickToTime(tick)) == tick,
                             "round trip for denominator");
            }
        }
        return ok;
    }

    bool testBarAnchoredSnapping() {
        bool ok = true;
        // Degenerate equivalence: a single 4/4 timeline snaps exactly like the
        // plain global grid for every grid-typical step.
        const Timeline single({
            {0, 120.0}
        });
        for (const int step : {1, 15, 30, 60, 120, 240, 480, 1920, 3840, 7680}) {
            for (int tick = 0; tick <= 4 * 1920; tick += 37) {
                ok &= expect(TimelineSnapUtils::snapNearest(tick, step, single) ==
                                 TimelineSnapUtils::snapNearest(tick, step),
                             "degenerate snapNearest equals global snapping");
                ok &= expect(TimelineSnapUtils::snapDown(tick, step, single) ==
                                 TimelineSnapUtils::snapDown(tick, step),
                             "degenerate snapDown equals global snapping");
            }
        }

        // 4/4 for bars 0..3, 3/4 for bars 4..7 (bar 4 at 7680), 6/8 from
        // bar 8 on (bar 8 at 13440).
        const auto timeline = threeMeterTimeline();
        // Beat snapping anchors on the containing measure's start.
        ok &= expect(TimelineSnapUtils::snapNearest(7680 + 500, 480, timeline) == 7680 + 480,
                     "beat snapping anchors on the 3/4 measure start");
        // Crossing the signature change from the left: the measure line wins.
        ok &= expect(TimelineSnapUtils::snapNearest(7680 - 100, 480, timeline) == 7680,
                     "snapping across a signature change reaches the measure line");
        // The next measure line is a target even when the step does not
        // divide the measure evenly (step 480 inside a 1440-tick 6/8 measure).
        ok &= expect(TimelineSnapUtils::snapNearest(13440 + 1400, 480, timeline) == 13440 + 1440,
                     "measure line wins when closer than the last step");
        // Measure-sized steps snap in whole measures despite uneven widths.
        ok &= expect(TimelineSnapUtils::snapNearest(8000, 1440, timeline) == 7680,
                     "measure-level snapping in the 3/4 section");
        ok &= expect(TimelineSnapUtils::snapDown(9200, 1440, timeline) == 7680 + 1440,
                     "snapDown lands on the measure start");
        // Negative ticks fall back to the plain grid instead of crashing.
        ok &= expect(TimelineSnapUtils::snapNearest(-5, 480, timeline) ==
                         TimelineSnapUtils::snapNearest(-5, 480),
                     "negative ticks use the plain grid");
        return ok;
    }

    bool testMusicTimeStrings() {
        bool ok = true;
        ok &= expect(MusicTime(0, 0, 0).toString() == QStringLiteral("001:01:000"),
                     "toString pads to 3/2/3 digits");
        ok &= expect(MusicTime(14, 1, 0).toString() == QStringLiteral("015:02:000"),
                     "toString is 1-based for measure and beat");
        bool parseOk = false;
        auto parsed = MusicTime::fromString(QStringLiteral("015:2:000"), &parseOk);
        ok &= expect(parseOk && parsed == MusicTime(14, 1, 0), "fromString parses 015:2:000");
        parsed = MusicTime::fromString(QStringLiteral("3"), &parseOk);
        ok &= expect(parseOk && parsed == MusicTime(2, 0, 0), "fromString defaults beat and tick");
        parsed = MusicTime::fromString(QStringLiteral("2：3：120"), &parseOk);
        ok &= expect(parseOk && parsed == MusicTime(1, 2, 120), "fromString accepts full-width colons");
        MusicTime::fromString(QStringLiteral("abc"), &parseOk);
        ok &= expect(!parseOk, "fromString rejects garbage");
        MusicTime::fromString(QStringLiteral(""), &parseOk);
        ok &= expect(!parseOk, "fromString rejects empty input");
        MusicTime::fromString(QStringLiteral("1:2:3:4"), &parseOk);
        ok &= expect(!parseOk, "fromString rejects too many components");
        // toString -> fromString round trip.
        const MusicTime original(41, 2, 337);
        parsed = MusicTime::fromString(original.toString(), &parseOk);
        ok &= expect(parseOk && parsed == original, "toString/fromString round trip");
        return ok;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    bool ok = true;

    ok &= testSinglePointMatchesLegacyConverter();
    ok &= testDegenerateEquivalence();
    ok &= testTickMsRoundTrip();
    ok &= testTempoQueries();
    ok &= testBarTickMapping();
    ok &= testTickTimeRoundTrip();
    ok &= testZeroPointInvariant();
    ok &= testMutationApi();
    ok &= testExtremeValues();
    ok &= testBarAnchoredSnapping();
    ok &= testMusicTimeStrings();

    if (!ok)
        return 1;
    std::printf("TestMusicTimeline passed\n");
    return 0;
}
