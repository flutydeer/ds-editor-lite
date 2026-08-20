#include <lite/MusicBase/Timeline.h>
#include <lite/Support/MathUtils.h>
#include "Modules/Inference/Utils/PitchRouting.h"

#include <QCoreApplication>
#include <QDebug>

#include <cmath>

namespace {

    void expect(const bool condition, const char *message) {
        if (!condition)
            qFatal("%s", message);
    }

    QList<double> curveSeconds(const Timeline &timeline, const int startTick, const int count) {
        QList<double> result;
        result.reserve(count);
        const double origin = timeline.tickToSec(startTick);
        for (int i = 0; i < count; ++i)
            result.append(timeline.tickToSec(startTick + i * 5) - origin);
        return result;
    }

    QList<double> frameSeconds(const int frames) {
        QList<double> result;
        result.reserve(frames);
        for (int i = 0; i < frames; ++i)
            result.append(i * 0.01);
        return result;
    }

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    const QList<double> targets{0.0, 0.5, 1.0, 1.5};
    expect(MathUtils::resample({}, {}, targets).isEmpty(), "empty source must stay empty");
    expect(MathUtils::resample({7.0}, {0.0}, targets) == QList<double>(4, 7.0),
           "single source value must fill every target");
    expect(MathUtils::resample({0.0, 10.0}, {0.0, 1.0}, targets) ==
               QList<double>({0.0, 5.0, 10.0, 10.0}),
           "explicit resampling must interpolate and clamp endpoints");

    const Timeline stepped({
        {0,   120.0},
        {480, 60.0 },
        {960, 180.0}
    });
    const Timeline sameValue({
        {0,   120.0},
        {480, 120.0}
    });
    const Timeline single({
        {0, 120.0}
    });
    const int startTick = 240;
    const int pointCount = 241;
    QList<double> values;
    values.reserve(pointCount);
    for (int i = 0; i < pointCount; ++i)
        values.append(std::sin(i / 20.0));

    const auto frameTargets = frameSeconds(220);
    const auto steppedFrames =
        MathUtils::resample(values, curveSeconds(stepped, startTick, pointCount), frameTargets);
    expect(steppedFrames.size() == frameTargets.size(),
           "stepped-tempo output length must equal requested frames");

    const auto singleFrames =
        MathUtils::resample(values, curveSeconds(single, startTick, pointCount), frameTargets);
    const auto sameFrames =
        MathUtils::resample(values, curveSeconds(sameValue, startTick, pointCount), frameTargets);
    expect(singleFrames == sameFrames, "same-value tempo points must be degenerate-equivalent");

    const auto boundaryTargets =
        QList<double>{stepped.tickToSec(480) - stepped.tickToSec(startTick),
                      stepped.tickToSec(960) - stepped.tickToSec(startTick)};
    const auto boundaryValues =
        MathUtils::resample(values, curveSeconds(stepped, startTick, pointCount), boundaryTargets);
    expect(boundaryValues.size() == 2, "tempo-boundary targets must not be truncated");

    expect(PitchRouting::applyToneShift({60.0, 61.0}, {1200.0, -50.0}) ==
               QList<double>({72.0, 60.5}),
           "tone shift must be added to MIDI pitch in cents");
    expect(PitchRouting::applyToneShift({60.0}, {100.0, 200.0}).isEmpty(),
           "mismatched pitch and tone-shift samples must fail");
    expect(PitchRouting::applyToneShift({0.0, -1.0, 60.0}, {1200.0, 1200.0, 1200.0}) ==
               QList<double>({0.0, -1.0, 72.0}),
           "tone shift must preserve non-positive unvoiced pitch samples");

    const auto f0 = PitchRouting::midiPitchToF0({69.0, 81.0}, 1.0, 4, 0.5);
    expect(f0.size() == 4, "vocoder f0 must use the requested frame count");
    expect(std::abs(f0.at(0) - 440.0f) < 0.01f, "MIDI 69 must map to 440 Hz");
    expect(std::abs(f0.at(1) - 622.25397f) < 0.01f,
           "vocoder f0 must interpolate the original pitch timeline");
    expect(std::abs(f0.at(2) - 880.0f) < 0.01f && std::abs(f0.at(3) - 880.0f) < 0.01f,
           "vocoder f0 must clamp the original pitch at the tail");
    expect(PitchRouting::midiPitchToF0({0.0, -1.0, 69.0}, 1.0, 3, 1.0) ==
               QList<float>({0.0f, 0.0f, 440.0f}),
           "vocoder f0 must preserve non-positive unvoiced pitch samples");
    return 0;
}
