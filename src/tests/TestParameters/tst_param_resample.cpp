#include "tst_parameters.h"

#include <QtTest/QTest>
#include <lite/MusicBase/Timeline.h>
#include <lite/Support/MathUtils.h>
#include "Modules/Inference/Utils/PitchRouting.h"
#include "Modules/Inference/Utils/PitchSampling.h"
#include "Modules/Inference/Models/InferInputBase.h"

#include <QCoreApplication>
#include <QDebug>

#include <cmath>

namespace {



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

void ParametersTests::paramResampleInterpolatesAndClampsEndpoints() {
    const QList<double> targets{0.0, 0.5, 1.0, 1.5};
    QVERIFY2((MathUtils::resample({}, {}, targets).isEmpty()), "empty source must stay empty");
    QVERIFY2((MathUtils::resample({7.0}, {0.0}, targets) == QList<double>(4, 7.0)),
             "single source value must fill every target");
    QVERIFY2((MathUtils::resample({0.0, 10.0}, {0.0, 1.0}, targets) ==
              QList<double>({0.0, 5.0, 10.0, 10.0})),
             "explicit resampling must interpolate and clamp endpoints");
}

void ParametersTests::paramResampleTempoAwareFrames() {
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
    QVERIFY2((steppedFrames.size() == frameTargets.size()),
             "stepped-tempo output length must equal requested frames");

    const auto singleFrames =
        MathUtils::resample(values, curveSeconds(single, startTick, pointCount), frameTargets);
    const auto sameFrames =
        MathUtils::resample(values, curveSeconds(sameValue, startTick, pointCount), frameTargets);
    QVERIFY2((singleFrames == sameFrames), "same-value tempo points must be degenerate-equivalent");

    const auto boundaryTargets =
        QList<double>{stepped.tickToSec(480) - stepped.tickToSec(startTick),
                      stepped.tickToSec(960) - stepped.tickToSec(startTick)};
    const auto boundaryValues =
        MathUtils::resample(values, curveSeconds(stepped, startTick, pointCount), boundaryTargets);
    QVERIFY2((boundaryValues.size() == 2), "tempo-boundary targets must not be truncated");
}

void ParametersTests::paramResampleToneShiftPreservesUnvoicedSamples() {
    QVERIFY2((PitchRouting::applyToneShift({60.0, 61.0}, {1200.0, -50.0}) ==
              QList<double>({72.0, 60.5})),
             "tone shift must be added to MIDI pitch in cents");
    QVERIFY2((PitchRouting::applyToneShift({60.0}, {100.0, 200.0}).isEmpty()),
             "mismatched pitch and tone-shift samples must fail");
    QVERIFY2((PitchRouting::applyToneShift({0.0, -1.0, 60.0}, {1200.0, 1200.0, 1200.0}) ==
              QList<double>({0.0, -1.0, 72.0})),
             "tone shift must preserve non-positive unvoiced pitch samples");
}

void ParametersTests::paramResampleVocoderPitchInterpolation() {
    const auto f0 = PitchRouting::midiPitchToF0({69.0, 81.0}, 1.0, 4, 0.5);
    QVERIFY2((f0.size() == 4), "vocoder f0 must use the requested frame count");
    QVERIFY2((std::abs(f0.at(0) - 440.0f) < 0.01f), "MIDI 69 must map to 440 Hz");
    QVERIFY2((std::abs(f0.at(1) - 622.25397f) < 0.01f),
             "vocoder f0 must interpolate the original pitch timeline");
    QVERIFY2((std::abs(f0.at(2) - 880.0f) < 0.01f && std::abs(f0.at(3) - 880.0f) < 0.01f),
             "vocoder f0 must clamp the original pitch at the tail");
    QVERIFY2((PitchRouting::midiPitchToF0({0.0, -1.0, 69.0}, 1.0, 3, 1.0) ==
              QList<float>({0.0f, 0.0f, 440.0f})),
             "vocoder f0 must preserve non-positive unvoiced pitch samples");
    QCOMPARE(PitchRouting::midiPitchToF0({69.0, 0.0}, 1.0, 4, 0.3),
             QList<float>({440.0f, 440.0f, 0.0f, 0.0f}));
    const QList<double> pitch{60.0, 62.0, 0.0, 0.0, 64.0};
    const QList<double> frames{0.0, 1.0, 2.0, 3.0, 4.0};
    QCOMPARE(PitchSampling::resample(pitch, frames, {0.5, 1.25, 1.75, 2.5, 3.5, 3.75, 5.0}),
             QList<double>({61.0, 62.0, 0.0, 0.0, 0.0, 64.0, 64.0}));
    QCOMPARE(PitchSampling::resample(pitch, frames, {1.5}), QList<double>({62.0}));
    QVERIFY(PitchSampling::resample(pitch, {0.0, 1.0, 1.0, 2.0, 3.0}, {0.5}).isEmpty());
    InferInputBase input;
    input.timeline = Timeline({{0, 120.0}});
    input.clipStartTick = 0;
    input.pieceStartTick = 0;
    input.pieceEndTick = 480;
    InferParamCurve extracted;
    extracted.localStartTick = 0;
    for (int i = 0; i < 96; ++i)
        extracted.values.append(i < 30 ? 60.0 : i < 60 ? 0.0 : 64.0);
    const auto inputFrames = input.resamplePitchToFrames(extracted, 50, 0.01);
    QCOMPARE(inputFrames.size(), 50);
    for (const auto value : inputFrames)
        QVERIFY(value == 0.0 || value >= 60.0);
    QVERIFY(inputFrames.contains(0.0) && inputFrames.contains(60.0) && inputFrames.contains(64.0));
    QCOMPARE(input.resamplePitchToFrames({}, 3, 0.01), QList<double>(3, 0.0));
}
