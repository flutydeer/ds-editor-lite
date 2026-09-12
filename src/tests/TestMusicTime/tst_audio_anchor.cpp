#include "tst_music_time.h"

#include <lite/MusicBase/Timeline.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>

#include <QCoreApplication>
#include <QtTest/QTest>

#include <cmath>
#include <memory>
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

    struct Ticks {
        int start;
        int clipStart;
        int clipLen;
        int length;
    };

    Ticks ticksOf(const AudioClip &clip) {
        return {clip.start(), clip.clipStart(), clip.clipLen(), clip.length()};
    }

    bool sameTicks(const Ticks &a, const Ticks &b) {
        return a.start == b.start && a.clipStart == b.clipStart && a.clipLen == b.clipLen &&
               a.length == b.length;
    }

    // The compensation triplet fed to talcs (mirrors AudioContext::feedCompensatedPosition)
    struct Triplet {
        int start;
        int clipStart;
        int clipLen;
    };

    Triplet tripletFor(const AudioClip &clip, const Timeline &timeline) {
        const int visibleStart = clip.start() + clip.clipStart();
        const int compClipStart =
            std::max(0, static_cast<int>(std::lround(timeline.msToTick(clip.trimStartMs()))));
        const double visibleMs = timeline.tickToMs(visibleStart);
        const int compClipLen = std::max(
            1, static_cast<int>(std::lround(timeline.msToTick(visibleMs + clip.playLengthMs()))) -
                   visibleStart);
        return {visibleStart - compClipStart, compClipStart, compClipLen};
    }

    std::unique_ptr<AudioClip> makeClip(const int start, const int clipStart, const int clipLen,
                                        const int length, const Timeline &timeline) {
        auto clip = std::make_unique<AudioClip>();
        clip->setStart(start);
        clip->setClipStart(clipStart);
        clip->setClipLen(clipLen);
        clip->setLength(length);
        clip->syncTruthFromTicks(timeline);
        return clip;
    }

}

// Any multi-point timeline whose points share one value must behave exactly
// like the single-point timeline.
void MusicTimeTests::audioAnchorDegenerateEquivalence() {
    const Timeline single({
        {0, 120.0}
    });
    const Timeline degenerate({
        {0,    120.0},
        {4800, 120.0},
        {9600, 120.0}
    });
    const int positions[] = {0, 480, 4800, 4801, 7200, 9600, 12000};
    for (const int p : positions) {
        const auto clip = makeClip(p, 240, 960, 1920, single);
        const Ticks before = ticksOf(*clip);
        QVERIFY2((!clip->updateTicksFromTruth(degenerate)),
                 "degenerate timeline leaves tick caches unchanged");
        QVERIFY2((sameTicks(before, ticksOf(*clip))), "degenerate timeline: tick caches identical");
        const auto tripletSingle = tripletFor(*clip, single);
        const auto tripletDegenerate = tripletFor(*clip, degenerate);
        QVERIFY2((tripletSingle.start == tripletDegenerate.start &&
                  tripletSingle.clipStart == tripletDegenerate.clipStart &&
                  tripletSingle.clipLen == tripletDegenerate.clipLen),
                 "degenerate timeline: compensation triplet identical");
    }
}

// sync + update under the same timeline must be a no-op, including across
// tempo change points.
void MusicTimeTests::audioAnchorRoundTripIdentity() {
    const Timeline maps[] = {
        Timeline({{0, 120.0}}
        ),
        Timeline({{0, 120.0}, {4800, 60.0}}
        ),
        Timeline({{0, 90.0}, {1920, 180.0}, {7680, 33.34}}
        ),
    };
    for (const auto &timeline : maps) {
        for (const int p : {0, 1920, 4799, 4800, 4801, 9000}) {
            const auto clip = makeClip(p, 480, 1920, 4800, timeline);
            const Ticks before = ticksOf(*clip);
            QVERIFY2((!clip->updateTicksFromTruth(timeline)),
                     "same-timeline round trip changes nothing");
            QVERIFY2((sameTicks(before, ticksOf(*clip))),
                     "same-timeline round trip: ticks identical");
        }
    }
}

// A real tempo change keeps the visible start tick and the realtime truth,
// and rescales the tick caches.
void MusicTimeTests::audioAnchorRealTempoChange() {
    const Timeline at120({
        {0, 120.0}
    });
    const Timeline at60({
        {0, 60.0}
    });
    // 960 ticks at 120 BPM = 1000 ms
    const auto clip = makeClip(4800, 0, 960, 960, at120);
    const double trim = clip->trimStartMs();
    const double playLen = clip->playLengthMs();
    QVERIFY(expectNear(playLen, 1000.0, "960 ticks at 120 BPM is 1000 ms"));

    QVERIFY2((clip->updateTicksFromTruth(at60)), "tempo change reports tick changes");
    QCOMPARE((clip->start() + clip->clipStart()), (4800));
    QVERIFY(expectNear(clip->trimStartMs(), trim, "trim is invariant across tempo changes"));
    QVERIFY(
        expectNear(clip->playLengthMs(), playLen, "play length is invariant across tempo changes"));
    // 1000 ms at 60 BPM = 480 ticks
    QCOMPARE((clip->clipLen()), (480));

    // And back: caches restore exactly
    QVERIFY2((clip->updateTicksFromTruth(at120)), "reverting the tempo changes ticks back");
    QCOMPARE((clip->clipLen()), (960));
}

// The triplet must cancel talcs' absolute conversion of clipStart: converting
// the compensated values through the map yields the intended realtime values.
void MusicTimeTests::audioAnchorCompensationProperty() {
    const Timeline timeline({
        {0,    120.0},
        {4800, 60.0 }
    });

    // Visible start inside the 60 BPM segment, trim and window crossing values
    const struct {
        int p;
        double trimMs;
        double playMs;
    } cases[] = {
        {7200, 500.0,  2000.0},
        {7200, 6000.0, 2000.0}, // trim maps beyond the first segment
        {4800, 0.0,    1000.0}, // visible start exactly on the tempo point
        {4000, 0.0,    2000.0}, // window crosses the tempo point
        {0,    0.0,    1000.0},
    };

    for (const auto &c : cases) {
        const auto clip = std::make_unique<AudioClip>();
        clip->setStart(c.p);
        clip->setClipStart(0);
        clip->setClipLen(1);
        clip->setLength(1);
        clip->setRealTimeAnchor(c.trimMs, c.playMs, c.trimMs + c.playMs);
        clip->updateTicksFromTruth(timeline);
        QCOMPARE((clip->start() + clip->clipStart()), (c.p));

        const auto triplet = tripletFor(*clip, timeline);
        // Half a tick of tolerance: the talcs interface is integer ticks
        const double halfTickMs =
            0.5 * (timeline.tickToMs(triplet.clipStart + 1) - timeline.tickToMs(triplet.clipStart));
        // readOffset = convertTime(clipStart') must equal the trim
        QVERIFY(expectNear(timeline.tickToMs(triplet.clipStart), c.trimMs,
                           "convertTime(clipStart') equals the material trim", halfTickMs + 1e-9));
        // start' + clipStart' = P exactly
        QCOMPARE((triplet.start + triplet.clipStart), (c.p));
        // len = convertTime(P + clipLen') - convertTime(P) must equal playLength
        const double lenMs = timeline.tickToMs(c.p + triplet.clipLen) - timeline.tickToMs(c.p);
        const double halfTickEndMs = 0.5 * (timeline.tickToMs(c.p + triplet.clipLen + 1) -
                                            timeline.tickToMs(c.p + triplet.clipLen));
        QVERIFY(expectNear(lenMs, c.playMs, "converted clip length equals the play length",
                           halfTickEndMs + 1e-9));
    }
}

// Properties round trip used by the undo actions.
void MusicTimeTests::audioAnchorPropertiesRoundTrip() {
    const Timeline timeline({
        {0,    120.0},
        {4800, 60.0 }
    });
    const auto clip = makeClip(5000, 480, 1920, 4800, timeline);
    Clip::ClipCommonProperties args;
    args.start = clip->start();
    args.clipStart = clip->clipStart();
    args.clipLen = clip->clipLen();
    args.length = clip->length();
    AudioClip::deriveTruthForProperties(args, timeline);
    QVERIFY(expectNear(args.trimStartMs, clip->trimStartMs(),
                       "deriveTruthForProperties matches syncTruthFromTicks"));
    QVERIFY(expectNear(args.playLengthMs, clip->playLengthMs(),
                       "deriveTruthForProperties play length matches"));

    const Ticks before = ticksOf(*clip);
    clip->applyRealTimeAnchorFromProperties(args, timeline);
    QVERIFY2((sameTicks(before, ticksOf(*clip))),
             "applying properties under the same timeline is a no-op");
}

// A pure move across a tempo boundary must keep the realtime window; only
// the components the tick edit changed may be re-derived.
void MusicTimeTests::audioAnchorMovePreservesTruth() {
    const Timeline timeline({
        {0,    120.0},
        {9600, 60.0 }
    });
    // Trimmed clip in the 120 BPM region: 4800 ticks of trim = 5000 ms
    const auto clip = makeClip(0, 4800, 4800, 9600, timeline);
    QVERIFY(expectNear(clip->trimStartMs(), 5000.0, "trim under 120 BPM is 5000 ms"));

    // Simulate the drag commit: only start changes (pure move into 60 BPM)
    Clip::ClipCommonProperties oldArgs;
    oldArgs.start = clip->start();
    oldArgs.clipStart = clip->clipStart();
    oldArgs.clipLen = clip->clipLen();
    oldArgs.length = clip->length();
    oldArgs.trimStartMs = clip->trimStartMs();
    oldArgs.playLengthMs = clip->playLengthMs();
    oldArgs.materialLengthMs = clip->materialLengthMs();

    auto newArgs = oldArgs;
    newArgs.start = 9600;
    newArgs.trimStartMs = -1;
    newArgs.playLengthMs = -1;
    newArgs.materialLengthMs = -1;
    AudioClip::deriveTruthForProperties(newArgs, timeline);
    AudioClip::preserveUnchangedTruth(newArgs, oldArgs);
    QVERIFY(
        expectNear(newArgs.trimStartMs, oldArgs.trimStartMs, "pure move keeps the material trim"));
    QVERIFY(
        expectNear(newArgs.playLengthMs, oldArgs.playLengthMs, "pure move keeps the play length"));
    QVERIFY(expectNear(newArgs.materialLengthMs, oldArgs.materialLengthMs,
                       "the material duration is never re-derived by an edit"));

    // A right trim redefines the play length but keeps the trim
    auto trimArgs = oldArgs;
    trimArgs.clipLen = 2400;
    trimArgs.trimStartMs = -1;
    trimArgs.playLengthMs = -1;
    trimArgs.materialLengthMs = -1;
    AudioClip::deriveTruthForProperties(trimArgs, timeline);
    AudioClip::preserveUnchangedTruth(trimArgs, oldArgs);
    QVERIFY(expectNear(trimArgs.trimStartMs, oldArgs.trimStartMs,
                       "right trim keeps the material trim"));
    QVERIFY2((std::abs(trimArgs.playLengthMs - oldArgs.playLengthMs) > 1.0),
             "right trim redefines the play length");
}

// The drag preview derives the tick caches from the gesture's ms truth;
// committing the same truth through the action path must reproduce them
// exactly (no jump on mouse release).
void MusicTimeTests::audioAnchorDragPreviewMatchesCommit() {
    const Timeline timeline({
        {0,    120.0},
        {4800, 60.0 },
        {9600, 150.0},
    });

    const struct {
        double trimMs;
        double playMs;
        double materialMs;
        int visibleStart;
    } cases[] = {
        {0.0,    2000.0, 4000.0, 0    },
        {500.0,  1500.0, 4000.0, 4700 }, // window crosses the first tempo point
        {5000.0, 2500.0, 9000.0, 9600 }, // trim spans two segments, start on a point
        {250.0,  3000.0, 3250.0, 12000}, // window ends exactly at the material end
    };

    for (const auto &c : cases) {
        const auto caches =
            AudioClip::deriveTickCaches(c.trimMs, c.playMs, c.materialMs, c.visibleStart, timeline);
        QCOMPARE((caches.start + caches.clipStart), (c.visibleStart));

        const auto clip = std::make_unique<AudioClip>();
        clip->setStart(caches.start);
        clip->setClipStart(caches.clipStart);
        clip->setClipLen(caches.clipLen);
        clip->setLength(caches.length);
        Clip::ClipCommonProperties args;
        args.start = caches.start;
        args.clipStart = caches.clipStart;
        args.clipLen = caches.clipLen;
        args.length = caches.length;
        args.trimStartMs = c.trimMs;
        args.playLengthMs = c.playMs;
        args.materialLengthMs = c.materialMs;
        clip->applyRealTimeAnchorFromProperties(args, timeline);
        QVERIFY2((clip->start() == caches.start && clip->clipStart() == caches.clipStart &&
                  clip->clipLen() == caches.clipLen && clip->length() == caches.length),
                 "committing the gesture truth reproduces the preview ticks");
        QVERIFY(expectNear(clip->trimStartMs(), c.trimMs,
                           "the committed trim is the gesture value, not a re-derivation"));
        QVERIFY(expectNear(clip->playLengthMs(), c.playMs,
                           "the committed play length is the gesture value"));
    }
}
