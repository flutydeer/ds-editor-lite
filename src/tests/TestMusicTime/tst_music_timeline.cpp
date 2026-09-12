#include "tst_music_time.h"

#include <lite/MusicBase/MusicTime.h>
#include <lite/MusicBase/MusicTimeConverter.h>
#include <lite/MusicBase/Timeline.h>
#include <lite/MusicBase/TimelineSnapUtils.h>

#include <QCoreApplication>
#include <QtTest/QTest>

#include <cmath>
#include "../TestSupport/TestAssertions.h"

namespace {

    bool expectNear(const double actual, const double expected, const char *message,
                    const double epsilon = 1e-6,
                    const std::source_location location = std::source_location::current()) {
        return TestSupport::expect(std::abs(actual - expected) <= epsilon,
                                   QStringLiteral("%1 (actual %2, expected %3)")
                                       .arg(QString::fromUtf8(message))
                                       .arg(actual, 0, 'g', 17)
                                       .arg(expected, 0, 'g', 17),
                                   location);
    }

    // 4/4 for bars 0..3, 3/4 for bars 4..7, 6/8 from bar 8 on.
    Timeline threeMeterTimeline() {
        return Timeline(
            {
                {0, 120.0}
        },
            {TimeSignature(0, 4, 4), TimeSignature(4, 3, 4), TimeSignature(8, 6, 8)});
    }

}

// A single-point timeline must behave exactly like the legacy global
// converter, bit for bit.
void MusicTimeTests::musicTimelineSinglePointMatchesLegacyConverter() {
    const double tempi[] = {120.0, 127.3, 60.0, 240.0, 33.34};
    const double ticks[] = {0.0, 1.0, 5.0, 479.0, 480.0, 481.0, 960.5, 12345.678, -960.0};
    for (const auto tempo : tempi) {
        const Timeline timeline({
            {0, tempo}
        });
        for (const auto tick : ticks) {
            const auto viaTimeline = timeline.tickToMs(tick);
            const auto viaLegacy = MusicTimeConverter::tickToMs(tick, tempo);
            QVERIFY2((viaTimeline == viaLegacy),
                     "single-point tickToMs is bit-identical to legacy converter");
            const auto backViaTimeline = timeline.msToTick(viaTimeline);
            const auto backViaLegacy = MusicTimeConverter::msToTick(viaLegacy, tempo);
            QVERIFY2((backViaTimeline == backViaLegacy),
                     "single-point msToTick is bit-identical to legacy converter");
            QVERIFY2((timeline.tickToSec(tick) == MusicTimeConverter::tickToSec(tick, tempo)),
                     "single-point tickToSec is bit-identical to legacy converter");
        }
    }
    // Legacy formatted display for a 4/4 project.
    const Timeline timeline({
        {0, 120.0}
    });
    for (const int tick : {0, 1, 479, 480, 1919, 1920, 1921, 76800}) {
        QCOMPARE((timeline.getBarBeatTickTime(tick)),
                 (MusicTimeConverter::getBarBeatTickTime(tick, 4, 4)));
    }
}

// Degenerate equivalence: several same-valued points must behave exactly
// like a single point.
void MusicTimeTests::musicTimelineDegenerateEquivalence() {
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
    for (const double tick :
         {0.0, 1.5, 9599.0, 9600.0, 9601.0, 19199.5, 19200.0, 19201.0, 50000.25, -100.0}) {
        QVERIFY(expectNear(multi.tickToMs(tick), single.tickToMs(tick),
                           "degenerate tickToMs equals single point"));
        const auto ms = single.tickToMs(tick);
        QVERIFY(expectNear(multi.msToTick(ms), single.msToTick(ms),
                           "degenerate msToTick equals single point"));
    }
    for (const int tick : {0, 1, 7679, 7680, 7681, 23039, 23040, 23041, 76800}) {
        QCOMPARE((multi.tickToTime(tick)), (single.tickToTime(tick)));
        QCOMPARE((multi.getBarBeatTickTime(tick)), (single.getBarBeatTickTime(tick)));
    }
    for (int bar = 0; bar < 32; bar++) {
        QCOMPARE((multi.barToTick(bar)), (single.barToTick(bar)));
    }
}

void MusicTimeTests::musicTimelineTickMsRoundTrip() {
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
        QVERIFY2((ms > previousMs), "tickToMs is strictly increasing");
        previousMs = ms;
        QVERIFY(expectNear(timeline.msToTick(ms), tick, "tick -> ms -> tick round trip", 1e-6));
    }
    // ms -> tick -> ms round trip on a uniform sweep.
    for (double ms = 0.0; ms <= 60000.0; ms += 313.7) {
        QVERIFY(expectNear(timeline.tickToMs(timeline.msToTick(ms)), ms,
                           "ms -> tick -> ms round trip", 1e-6));
    }
    // Second-domain helpers are consistent with the ms-domain ones.
    QVERIFY(expectNear(timeline.tickToSec(3840.0), timeline.tickToMs(3840.0) / 1000.0,
                       "tickToSec consistent with tickToMs"));
    QVERIFY(expectNear(timeline.secToTick(2.5), timeline.msToTick(2500.0),
                       "secToTick consistent with msToTick"));
}

void MusicTimeTests::musicTimelineTempoQueries() {
    const Timeline timeline({
        {0,    120.0},
        {1920, 60.0 },
        {3840, 180.0}
    });
    QVERIFY(expectNear(timeline.tempoAt(0), 120.0, "tempoAt at tick 0"));
    QVERIFY(expectNear(timeline.tempoAt(1919), 120.0, "tempoAt just before a point"));
    QVERIFY(expectNear(timeline.tempoAt(1920), 60.0, "tempoAt exactly on a point"));
    QVERIFY(expectNear(timeline.tempoAt(99999), 180.0, "tempoAt after the last point"));
    QCOMPARE((timeline.nearestTickWithTempoTo(0)), (0));
    QCOMPARE((timeline.nearestTickWithTempoTo(1919)), (0));
    QCOMPARE((timeline.nearestTickWithTempoTo(1920)), (1920));
    QCOMPARE((timeline.nearestTickWithTempoTo(5000)), (3840));
}

void MusicTimeTests::musicTimelineBarTickMapping() {
    const auto timeline = threeMeterTimeline();
    // 4/4 bars are 1920 ticks, 3/4 bars 1440 ticks, 6/8 bars 6 * 240 = 1440 ticks.
    QCOMPARE((timeline.barToTick(0)), (0));
    QCOMPARE((timeline.barToTick(1)), (1920));
    QCOMPARE((timeline.barToTick(4)), (7680));
    QCOMPARE((timeline.barToTick(5)), (7680 + 1440));
    QCOMPARE((timeline.barToTick(8)), (7680 + 4 * 1440));
    QCOMPARE((timeline.barToTick(9)), (13440 + 1440));
    QCOMPARE((timeline.barToTick(-1)), (-1));

    QCOMPARE((timeline.timeSignatureAt(0).numerator), (4));
    QCOMPARE((timeline.timeSignatureAt(3).numerator), (4));
    QCOMPARE((timeline.timeSignatureAt(4).numerator), (3));
    QCOMPARE((timeline.timeSignatureAt(7).numerator), (3));
    QCOMPARE((timeline.timeSignatureAt(8).denominator), (8));
    QCOMPARE((timeline.timeSignatureAt(100).denominator), (8));
    QCOMPARE((timeline.nearestBarWithTimeSignatureTo(7)), (4));
    QCOMPARE((timeline.nearestBarWithTimeSignatureTo(8)), (8));
}

void MusicTimeTests::musicTimelineTickTimeRoundTrip() {
    const auto timeline = threeMeterTimeline();
    // Round trip across all three segments including boundaries +/- 1.
    for (const int tick : {0, 1, 1919, 1920, 7679, 7680, 7681, 9119, 9120, 13439, 13440, 13441,
                           13680, 14879, 14880, 99999}) {
        const auto time = timeline.tickToTime(tick);
        QVERIFY2((time.isValid()), "tickToTime yields a valid triple");
        QCOMPARE((timeline.timeToTick(time)), (tick));
    }
    // Spot checks for measure accumulation across meter changes.
    QCOMPARE((timeline.tickToTime(7680)), (MusicTime(4, 0, 0)));
    QCOMPARE((timeline.tickToTime(13440)), (MusicTime(8, 0, 0)));
    QCOMPARE((timeline.tickToTime(14000)), (MusicTime(8, 2, 80)));
    // barToTick(bar) == timeToTick(bar, 0, 0) for every bar.
    for (int bar = 0; bar < 64; bar++) {
        QCOMPARE((timeline.barToTick(bar)), (timeline.timeToTick(bar, 0, 0)));
    }
    // Beat overflow spills forward using the measure's own beat length.
    QCOMPARE((timeline.timeToTick(0, 4, 0)), (1920));
    // Invalid inputs return -1 instead of crashing.
    QCOMPARE((timeline.timeToTick(-1, 0, 0)), (-1));
    QCOMPARE((timeline.timeToTick(0, -1, 0)), (-1));
    QCOMPARE((timeline.timeToTick(0, 0, -1)), (-1));
    QVERIFY2((!timeline.tickToTime(-5).isValid()), "negative tick yields invalid time");
}

void MusicTimeTests::musicTimelineZeroPointInvariant() {
    Timeline timeline(
        {
            {0,    120.0},
            {1920, 60.0 }
    },
        {TimeSignature(0, 4, 4), TimeSignature(4, 3, 4)});
    QVERIFY2((!timeline.removeTempoAt(0)), "tempo point at tick 0 is not removable");
    QVERIFY2((!timeline.removeTimeSignatureAt(0)), "signature at bar 0 is not removable");
    QCOMPARE((timeline.tempos().size()), (2));
    QCOMPARE((timeline.timeSignatures().size()), (2));
    QVERIFY(expectNear(timeline.tempoAt(0), 120.0, "conversions intact after refused removal"));

    QVERIFY2((timeline.removeTempoAt(1920)), "removing an existing tempo point succeeds");
    QVERIFY2((!timeline.removeTempoAt(1920)), "removing a missing tempo point fails");
    QVERIFY2((timeline.removeTimeSignatureAt(4)), "removing an existing signature succeeds");
    QVERIFY2((timeline.tempos().size() == 1 && timeline.timeSignatures().size() == 1),
             "lists shrink after successful removal");

    // A timeline built without a zero point anchors the earliest point at 0.
    const Timeline anchored({
        {960, 90.0}
    });
    QVERIFY2((anchored.tempos().size() == 2 && anchored.tempos().first().pos == 0),
             "missing zero tempo point is anchored");
    QVERIFY(expectNear(anchored.tempoAt(0), 90.0, "anchored point copies the earliest value"));
    const Timeline empty(QList<Tempo>{}, QList<TimeSignature>{});
    QVERIFY2((empty.tempos().size() == 1 && empty.tempos().first().pos == 0),
             "empty tempo list falls back to the default point");
    QVERIFY2((empty.timeSignatures().size() == 1 && empty.timeSignatures().first().barIndex == 0),
             "empty signature list falls back to the default point");
}

void MusicTimeTests::musicTimelineMutationApi() {
    Timeline timeline;
    timeline.addTempo({1920, 60.0});
    timeline.addTempo({960, 90.0});
    QCOMPARE((timeline.tempos().size()), (3));
    QCOMPARE((timeline.tempos()[1].pos), (960));
    timeline.addTempo({960, 95.0});
    QCOMPARE((timeline.tempos().size()), (3));
    QVERIFY(expectNear(timeline.tempoAt(960), 95.0, "replaced tempo value is effective"));

    timeline.addTimeSignature(TimeSignature(4, 3, 4));
    timeline.addTimeSignature(TimeSignature(2, 6, 8));
    QCOMPARE((timeline.timeSignatures().size()), (3));
    QCOMPARE((timeline.timeSignatures()[1].barIndex), (2));
    QCOMPARE((timeline.barToTick(3)), (2 * 1920 + 1440));

    // Duplicate positions in bulk input keep the first occurrence.
    Timeline duplicated({
        {0,   120.0},
        {960, 140.0},
        {960, 150.0}
    });
    QCOMPARE((duplicated.tempos().size()), (2));
    QVERIFY(expectNear(duplicated.tempoAt(960), 140.0, "first duplicate wins"));
}

void MusicTimeTests::musicTimelineExtremeValues() {
    // Extremely slow and fast tempi keep the round trip stable.
    const Timeline extremes({
        {0,    1.0   },
        {1920, 999.0 },
        {3840, 20.5  },
        {5760, 500.25}
    });
    for (const double tick : {0.0, 1919.0, 1920.0, 3839.5, 3840.0, 5760.0, 100000.0}) {
        QVERIFY(expectNear(extremes.msToTick(extremes.tickToMs(tick)), tick,
                           "extreme tempo round trip", 1e-6));
    }
    // All power-of-two denominators up to 128 (and denominator 1).
    for (const int denominator : {1, 2, 4, 8, 16, 32, 64, 128}) {
        const Timeline timeline(
            {
                {0, 120.0}
        },
            {TimeSignature(0, 3, denominator)});
        const int barTicks = 3 * (MusicTime::ticksPerWholeNote / denominator);
        QCOMPARE((timeline.barToTick(5)), (5 * barTicks));
        for (const int tick : {0, barTicks - 1, barTicks, barTicks + 1, 10 * barTicks + 7}) {
            QCOMPARE((timeline.timeToTick(timeline.tickToTime(tick))), (tick));
        }
    }
}

void MusicTimeTests::musicTimelineBarAnchoredSnapping() {
    // Degenerate equivalence: a single 4/4 timeline snaps exactly like the
    // plain global grid for every grid-typical step.
    const Timeline single({
        {0, 120.0}
    });
    for (const int step : {1, 15, 30, 60, 120, 240, 480, 1920, 3840, 7680}) {
        for (int tick = 0; tick <= 4 * 1920; tick += 37) {
            QCOMPARE((TimelineSnapUtils::snapNearest(tick, step, single)),
                     (TimelineSnapUtils::snapNearest(tick, step)));
            QCOMPARE((TimelineSnapUtils::snapDown(tick, step, single)),
                     (TimelineSnapUtils::snapDown(tick, step)));
        }
    }

    // 4/4 for bars 0..3, 3/4 for bars 4..7 (bar 4 at 7680), 6/8 from
    // bar 8 on (bar 8 at 13440).
    const auto timeline = threeMeterTimeline();
    // Beat snapping anchors on the containing measure's start.
    QCOMPARE((TimelineSnapUtils::snapNearest(7680 + 500, 480, timeline)), (7680 + 480));
    // Crossing the signature change from the left: the measure line wins.
    QCOMPARE((TimelineSnapUtils::snapNearest(7680 - 100, 480, timeline)), (7680));
    // The next measure line is a target even when the step does not
    // divide the measure evenly (step 480 inside a 1440-tick 6/8 measure).
    QCOMPARE((TimelineSnapUtils::snapNearest(13440 + 1400, 480, timeline)), (13440 + 1440));
    // Measure-sized steps snap in whole measures despite uneven widths.
    QCOMPARE((TimelineSnapUtils::snapNearest(8000, 1440, timeline)), (7680));
    QCOMPARE((TimelineSnapUtils::snapDown(9200, 1440, timeline)), (7680 + 1440));
    // Negative ticks fall back to the plain grid instead of crashing.
    QCOMPARE((TimelineSnapUtils::snapNearest(-5, 480, timeline)),
             (TimelineSnapUtils::snapNearest(-5, 480)));
}

void MusicTimeTests::musicTimelineMusicTimeStrings() {
    QCOMPARE((MusicTime(0, 0, 0).toString()), (QStringLiteral("001:01:000")));
    QCOMPARE((MusicTime(14, 1, 0).toString()), (QStringLiteral("015:02:000")));
    bool parseOk = false;
    auto parsed = MusicTime::fromString(QStringLiteral("015:2:000"), &parseOk);
    QVERIFY2((parseOk && parsed == MusicTime(14, 1, 0)), "fromString parses 015:2:000");
    parsed = MusicTime::fromString(QStringLiteral("3"), &parseOk);
    QVERIFY2((parseOk && parsed == MusicTime(2, 0, 0)), "fromString defaults beat and tick");
    parsed = MusicTime::fromString(QStringLiteral("2：3：120"), &parseOk);
    QVERIFY2((parseOk && parsed == MusicTime(1, 2, 120)), "fromString accepts full-width colons");
    MusicTime::fromString(QStringLiteral("abc"), &parseOk);
    QVERIFY2((!parseOk), "fromString rejects garbage");
    MusicTime::fromString(QStringLiteral(""), &parseOk);
    QVERIFY2((!parseOk), "fromString rejects empty input");
    MusicTime::fromString(QStringLiteral("1:2:3:4"), &parseOk);
    QVERIFY2((!parseOk), "fromString rejects too many components");
    // toString -> fromString round trip.
    const MusicTime original(41, 2, 337);
    parsed = MusicTime::fromString(original.toString(), &parseOk);
    QVERIFY2((parseOk && parsed == original), "toString/fromString round trip");
}
