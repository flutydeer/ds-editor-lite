#include <lite/MusicBase/Timeline.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>

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

    AudioClip *makeClip(const int start, const int clipStart, const int clipLen, const int length,
                        const Timeline &timeline) {
        const auto clip = new AudioClip;
        clip->setStart(start);
        clip->setClipStart(clipStart);
        clip->setClipLen(clipLen);
        clip->setLength(length);
        clip->syncTruthFromTicks(timeline);
        return clip;
    }

    // Any multi-point timeline whose points share one value must behave exactly
    // like the single-point timeline.
    bool testDegenerateEquivalence() {
        bool ok = true;
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
            ok &= expect(!clip->updateTicksFromTruth(degenerate),
                         "degenerate timeline leaves tick caches unchanged");
            ok &= expect(sameTicks(before, ticksOf(*clip)),
                         "degenerate timeline: tick caches identical");
            const auto tripletSingle = tripletFor(*clip, single);
            const auto tripletDegenerate = tripletFor(*clip, degenerate);
            ok &= expect(tripletSingle.start == tripletDegenerate.start &&
                             tripletSingle.clipStart == tripletDegenerate.clipStart &&
                             tripletSingle.clipLen == tripletDegenerate.clipLen,
                         "degenerate timeline: compensation triplet identical");
            delete clip;
        }
        return ok;
    }

    // sync + update under the same timeline must be a no-op, including across
    // tempo change points.
    bool testRoundTripIdentity() {
        bool ok = true;
        const Timeline maps[] = {
            Timeline({
                {0, 120.0}
            }),
            Timeline({
                {0,    120.0},
                {4800, 60.0 }
            }),
            Timeline({
                {0,    90.0 },
                {1920, 180.0},
                {7680, 33.34}
            }),
        };
        for (const auto &timeline : maps) {
            for (const int p : {0, 1920, 4799, 4800, 4801, 9000}) {
                const auto clip = makeClip(p, 480, 1920, 4800, timeline);
                const Ticks before = ticksOf(*clip);
                ok &= expect(!clip->updateTicksFromTruth(timeline),
                             "same-timeline round trip changes nothing");
                ok &= expect(sameTicks(before, ticksOf(*clip)),
                             "same-timeline round trip: ticks identical");
                delete clip;
            }
        }
        return ok;
    }

    // A real tempo change keeps the visible start tick and the realtime truth,
    // and rescales the tick caches.
    bool testRealTempoChange() {
        bool ok = true;
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
        ok &= expectNear(playLen, 1000.0, "960 ticks at 120 BPM is 1000 ms");

        ok &= expect(clip->updateTicksFromTruth(at60), "tempo change reports tick changes");
        ok &= expect(clip->start() + clip->clipStart() == 4800,
                     "visible start tick is invariant across tempo changes");
        ok &= expectNear(clip->trimStartMs(), trim, "trim is invariant across tempo changes");
        ok &= expectNear(clip->playLengthMs(), playLen,
                         "play length is invariant across tempo changes");
        // 1000 ms at 60 BPM = 480 ticks
        ok &= expect(clip->clipLen() == 480, "1000 ms at 60 BPM derives 480 ticks");

        // And back: caches restore exactly
        ok &= expect(clip->updateTicksFromTruth(at120), "reverting the tempo changes ticks back");
        ok &= expect(clip->clipLen() == 960, "reverting the tempo restores the tick caches");
        delete clip;
        return ok;
    }

    // The triplet must cancel talcs' absolute conversion of clipStart: converting
    // the compensated values through the map yields the intended realtime values.
    bool testCompensationProperty() {
        bool ok = true;
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
            const auto clip = new AudioClip;
            clip->setStart(c.p);
            clip->setClipStart(0);
            clip->setClipLen(1);
            clip->setLength(1);
            clip->setRealTimeAnchor(c.trimMs, c.playMs, c.trimMs + c.playMs);
            clip->updateTicksFromTruth(timeline);
            ok &= expect(clip->start() + clip->clipStart() == c.p,
                         "updateTicksFromTruth keeps the visible start");

            const auto triplet = tripletFor(*clip, timeline);
            // Half a tick of tolerance: the talcs interface is integer ticks
            const double halfTickMs = 0.5 * (timeline.tickToMs(triplet.clipStart + 1) -
                                             timeline.tickToMs(triplet.clipStart));
            // readOffset = convertTime(clipStart') must equal the trim
            ok &= expectNear(timeline.tickToMs(triplet.clipStart), c.trimMs,
                             "convertTime(clipStart') equals the material trim", halfTickMs + 1e-9);
            // start' + clipStart' = P exactly
            ok &= expect(triplet.start + triplet.clipStart == c.p,
                         "start' + clipStart' equals the visible start tick");
            // len = convertTime(P + clipLen') - convertTime(P) must equal playLength
            const double lenMs =
                timeline.tickToMs(c.p + triplet.clipLen) - timeline.tickToMs(c.p);
            const double halfTickEndMs = 0.5 * (timeline.tickToMs(c.p + triplet.clipLen + 1) -
                                                timeline.tickToMs(c.p + triplet.clipLen));
            ok &= expectNear(lenMs, c.playMs, "converted clip length equals the play length",
                             halfTickEndMs + 1e-9);
            delete clip;
        }
        return ok;
    }

    // Properties round trip used by the undo actions.
    bool testPropertiesRoundTrip() {
        bool ok = true;
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
        ok &= expectNear(args.trimStartMs, clip->trimStartMs(),
                         "deriveTruthForProperties matches syncTruthFromTicks");
        ok &= expectNear(args.playLengthMs, clip->playLengthMs(),
                         "deriveTruthForProperties play length matches");

        const Ticks before = ticksOf(*clip);
        clip->applyRealTimeAnchorFromProperties(args, timeline);
        ok &= expect(sameTicks(before, ticksOf(*clip)),
                     "applying properties under the same timeline is a no-op");
        delete clip;
        return ok;
    }

    // A pure move across a tempo boundary must keep the realtime window; only
    // the components the tick edit changed may be re-derived.
    bool testMovePreservesTruth() {
        bool ok = true;
        const Timeline timeline({
            {0,    120.0},
            {9600, 60.0 }
        });
        // Trimmed clip in the 120 BPM region: 4800 ticks of trim = 5000 ms
        const auto clip = makeClip(0, 4800, 4800, 9600, timeline);
        ok &= expectNear(clip->trimStartMs(), 5000.0, "trim under 120 BPM is 5000 ms");

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
        ok &= expectNear(newArgs.trimStartMs, oldArgs.trimStartMs,
                         "pure move keeps the material trim");
        ok &= expectNear(newArgs.playLengthMs, oldArgs.playLengthMs,
                         "pure move keeps the play length");
        ok &= expectNear(newArgs.materialLengthMs, oldArgs.materialLengthMs,
                         "the material duration is never re-derived by an edit");

        // A right trim redefines the play length but keeps the trim
        auto trimArgs = oldArgs;
        trimArgs.clipLen = 2400;
        trimArgs.trimStartMs = -1;
        trimArgs.playLengthMs = -1;
        trimArgs.materialLengthMs = -1;
        AudioClip::deriveTruthForProperties(trimArgs, timeline);
        AudioClip::preserveUnchangedTruth(trimArgs, oldArgs);
        ok &= expectNear(trimArgs.trimStartMs, oldArgs.trimStartMs,
                         "right trim keeps the material trim");
        ok &= expect(std::abs(trimArgs.playLengthMs - oldArgs.playLengthMs) > 1.0,
                     "right trim redefines the play length");
        delete clip;
        return ok;
    }

    // The drag preview derives the tick caches from the gesture's ms truth;
    // committing the same truth through the action path must reproduce them
    // exactly (no jump on mouse release).
    bool testDragPreviewMatchesCommit() {
        bool ok = true;
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
            const auto caches = AudioClip::deriveTickCaches(c.trimMs, c.playMs, c.materialMs,
                                                            c.visibleStart, timeline);
            ok &= expect(caches.start + caches.clipStart == c.visibleStart,
                         "preview keeps the visible start tick");

            const auto clip = new AudioClip;
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
            ok &= expect(clip->start() == caches.start && clip->clipStart() == caches.clipStart &&
                             clip->clipLen() == caches.clipLen && clip->length() == caches.length,
                         "committing the gesture truth reproduces the preview ticks");
            ok &= expectNear(clip->trimStartMs(), c.trimMs,
                             "the committed trim is the gesture value, not a re-derivation");
            ok &= expectNear(clip->playLengthMs(), c.playMs,
                             "the committed play length is the gesture value");
            delete clip;
        }
        return ok;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    bool ok = true;
    ok &= testDegenerateEquivalence();
    ok &= testRoundTripIdentity();
    ok &= testRealTempoChange();
    ok &= testCompensationProperty();
    ok &= testPropertiesRoundTrip();
    ok &= testMovePreservesTruth();
    ok &= testDragPreviewMatchesCommit();

    if (!ok)
        return 1;
    std::printf("TestAudioAnchor passed\n");
    return 0;
}
