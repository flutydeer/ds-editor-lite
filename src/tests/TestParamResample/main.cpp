#include <lite/MusicBase/Timeline.h>
#include <lite/Support/MathUtils.h>

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
    return 0;
}
