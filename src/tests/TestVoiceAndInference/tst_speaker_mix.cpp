#include "tst_voice_and_inference.h"

#include <lite/ProjectModel/AppModel/SpeakerMixData.h>
#include <lite/MusicBase/Timeline.h>
#include <lite/ProjectModel/InferenceData/InferSpeakerMix.h>
#include "UI/Utils/OverlappingHandleResolver.h"
#include "UI/Utils/SpeakerMixUtils.h"

#include <QCoreApplication>
#include <QtTest/QTest>

#include <cmath>

namespace {
    constexpr double kTolerance = 1e-9;

    bool compareVectorNear(const QVector<double> &actual, const QVector<double> &expected,
                           const char *file, const int line) {
        if (!QTest::qCompare(actual.size(), expected.size(), "actual.size()", "expected.size()",
                             file, line))
            return false;
        for (qsizetype index = 0; index < actual.size(); ++index) {
            const auto detail = QStringLiteral("index=%1 actual=%2 expected=%3")
                                    .arg(index)
                                    .arg(actual[index], 0, 'g', 17)
                                    .arg(expected[index], 0, 'g', 17);
            if (!QTest::qVerify(std::abs(actual[index] - expected[index]) <= kTolerance,
                                "weights are within tolerance", qPrintable(detail), file, line))
                return false;
        }
        return true;
    }

    SpeakerMixModel::SpeakerMixSource source(const QString &speakerId) {
        return SpeakerMixModel::SpeakerMixSource{SpeakerInfo(speakerId)};
    }

    SpeakerMixModel::SpeakerMixData fixedMixData() {
        SpeakerMixModel::SpeakerMixData data;
        data.mode = SpeakerMixModel::SingerSourceMode::FixedMix;
        data.sources = {source("spk-a"), source("spk-b"), source("spk-c")};
        data.fixedWeights = {0.2, 0.3};
        return data;
    }

    SpeakerMixModel::SpeakerMixData dynamicMixData() {
        SpeakerMixModel::SpeakerMixData data;
        data.mode = SpeakerMixModel::SingerSourceMode::DynamicMix;
        data.sources = {source("spk-a"), source("spk-b")};
        data.dynamicKeyframes = {
            {960, {1.0}},
            {0,   {0.0}}
        };
        return data;
    }

    Timeline timeline120Bpm() {
        // The default-constructed timeline is a single 120 BPM point plus 4/4.
        return Timeline();
    }

}

void VoiceAndInferenceTests::speakerMixWeightConversions() {
    using namespace SpeakerMixModel;

    if (!compareVectorNear(normalizeSpeakerMixFullWeights({-1.0, 2.0, 1.0}, 3), {0.0, 0.5, 0.5},
                           __FILE__, __LINE__))
        return;
    if (!compareVectorNear(normalizeSpeakerMixFullWeights({0.0, 0.0, 0.0}, 3),
                           {1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0}, __FILE__, __LINE__))
        return;
    QVERIFY2(normalizeSpeakerMixFullWeights({1.0}, 0).isEmpty(),
             "non-positive source count returns empty weights");
    if (!compareVectorNear(explicitWeightsFromFullWeights({2.0, 1.0, 1.0}), {1.0 / 3.0, 1.0 / 3.0},
                           __FILE__, __LINE__))
        return;
    if (!compareVectorNear(fullWeightsFromExplicitWeights({0.2, 0.3}), {0.2, 0.3, 0.5}, __FILE__,
                           __LINE__))
        return;
    if (!compareVectorNear(fullWeightsFromExplicitWeights({0.8, 0.8}), {0.5, 0.5, 0.0}, __FILE__,
                           __LINE__))
        return;
}

void VoiceAndInferenceTests::speakerMixOverlappingSplitResolution() {

    const QVector<double> positions{10.0, 10.0, 10.0, 20.0};
    QCOMPARE(OverlappingHandleResolver::resolve(positions, 1, -4.0), 0);
    QCOMPARE(OverlappingHandleResolver::resolve(positions, 1, 4.0), 2);
    QCOMPARE(OverlappingHandleResolver::resolve(positions, 3, -4.0), 3);

    const QVector<double> leadingZeros{0.0, 0.0, 0.4, 0.6};
    QCOMPARE(SpeakerMixUtils::resolveOverlappingSplitIndex(leadingZeros, 0, 0.1), 1);
    QCOMPARE(SpeakerMixUtils::resolveOverlappingSplitIndex(leadingZeros, 1, -0.1), 0);

    const QVector<double> trailingZeros{0.4, 0.6, 0.0, 0.0};
    QCOMPARE(SpeakerMixUtils::resolveOverlappingSplitIndex(trailingZeros, 2, -0.1), 1);
    QCOMPARE(SpeakerMixUtils::resolveOverlappingSplitIndex(trailingZeros, 1, 0.1), 2);

    const QVector<double> threeLeadingZeros{0.0, 0.0, 0.0, 0.4, 0.6};
    QCOMPARE(SpeakerMixUtils::resolveOverlappingSplitIndex(threeLeadingZeros, 1, -0.1), 0);
    QCOMPARE(SpeakerMixUtils::resolveOverlappingSplitIndex(threeLeadingZeros, 1, 0.1), 2);

    const QVector<double> threeTrailingZeros{0.4, 0.6, 0.0, 0.0, 0.0};
    QCOMPARE(SpeakerMixUtils::resolveOverlappingSplitIndex(threeTrailingZeros, 2, -0.1), 1);
    QCOMPARE(SpeakerMixUtils::resolveOverlappingSplitIndex(threeTrailingZeros, 2, 0.1), 3);

    const QVector<double> interiorZeros{0.2, 0.0, 0.0, 0.0, 0.8};
    QCOMPARE(SpeakerMixUtils::resolveOverlappingSplitIndex(interiorZeros, 2, -0.1), 0);
    QCOMPARE(SpeakerMixUtils::resolveOverlappingSplitIndex(interiorZeros, 2, 0.1), 3);

    const QVector<double> separateSplits{0.2, 0.3, 0.5};
    QCOMPARE(SpeakerMixUtils::resolveOverlappingSplitIndex(separateSplits, 1, -0.1), 1);
    QCOMPARE(SpeakerMixUtils::resolveOverlappingSplitIndex(separateSplits, 1, 0.0), 1);
}

void VoiceAndInferenceTests::speakerMixNormalizeSpeakerMixData() {
    using namespace SpeakerMixModel;


    auto fixed = fixedMixData();
    fixed.fixedWeights = {2.0, 1.0};
    fixed.dynamicKeyframes = {
        {240, {2.0, 1.0}}
    };
    fixed.sourcePresetId = " preset ";
    fixed.sourcePresetName = " name ";
    const auto normalizedFixed = SpeakerMixModel::normalizeSpeakerMixData(fixed);
    QCOMPARE(normalizedFixed.mode, SingerSourceMode::FixedMix);
    if (!compareVectorNear(normalizedFixed.fixedWeights, {0.5, 0.5}, __FILE__, __LINE__))
        return;
    QVERIFY2(normalizedFixed.dynamicKeyframes.isEmpty(),
             "fixed mix does not carry dynamic automation");
    QCOMPARE(normalizedFixed.sourcePresetId, "preset");
    QCOMPARE(normalizedFixed.sourcePresetName, "name");

    auto noPreset = fixedMixData();
    noPreset.sourcePresetName = "stale";
    noPreset.sourcePresetDirty = true;
    const auto normalizedNoPreset = SpeakerMixModel::normalizeSpeakerMixData(noPreset);
    QVERIFY2(normalizedNoPreset.sourcePresetName.isEmpty(),
             "preset name and dirty flag clear when preset id is empty");
    QVERIFY2(!normalizedNoPreset.sourcePresetDirty,
             "preset name and dirty flag clear when preset id is empty");

    auto invalidFixed = fixedMixData();
    invalidFixed.fixedWeights = {0.5};
    QCOMPARE(SpeakerMixModel::normalizeSpeakerMixData(invalidFixed).mode, SingerSourceMode::Single);

    auto emptySource = fixedMixData();
    emptySource.sources[1] = source("");
    QCOMPARE(SpeakerMixModel::normalizeSpeakerMixData(emptySource).mode, SingerSourceMode::Single);

    auto dynamic = dynamicMixData();
    const auto normalizedDynamic = SpeakerMixModel::normalizeSpeakerMixData(dynamic);
    QCOMPARE(normalizedDynamic.mode, SingerSourceMode::DynamicMix);
    QCOMPARE(normalizedDynamic.dynamicKeyframes.first().tick, 0);
    QCOMPARE(normalizedDynamic.dynamicKeyframes.last().tick, 960);

    auto invalidDynamic = dynamicMixData();
    invalidDynamic.dynamicKeyframes.first().weights = {0.2, 0.3};
    QCOMPARE(SpeakerMixModel::normalizeSpeakerMixData(invalidDynamic).mode,
             SingerSourceMode::Single);
}

void VoiceAndInferenceTests::speakerMixDynamicStatePredicates() {
    using namespace SpeakerMixModel;


    const auto dynamic = dynamicMixData();
    QVERIFY2(hasDynamicMixAutomation(dynamic), "dynamic mix reports automation");
    QVERIFY2(isDynamicMixActive(dynamic), "dynamic mix reports active");
    QVERIFY2(!isDynamicMixBypassed(dynamic), "dynamic mix is not bypassed");

    auto bypassed = dynamicMixData();
    bypassed.dynamicBypassed = true;
    bypassed.fixedWeights = {0.1};
    QVERIFY2(hasDynamicMixAutomation(bypassed), "bypassed dynamic mix reports automation");
    QVERIFY2(!isDynamicMixActive(bypassed), "bypassed dynamic mix is not active");
    QVERIFY2(isDynamicMixBypassed(bypassed), "bypassed dynamic mix reports bypassed");

    auto bypassedWithoutFixedWeights = dynamicMixData();
    bypassedWithoutFixedWeights.dynamicBypassed = true;
    if (!compareVectorNear(
            SpeakerMixModel::normalizeSpeakerMixData(bypassedWithoutFixedWeights).fixedWeights,
            {0.0}, __FILE__, __LINE__))
        return;

    auto legacyBypass = fixedMixData();
    legacyBypass.dynamicKeyframes = {
        {0, {0.1, 0.2}}
    };
    QVERIFY2(!hasDynamicMixAutomation(legacyBypass),
             "fixed mix with keyframes no longer means bypassed dynamic mix");
    QVERIFY2(!isDynamicMixBypassed(legacyBypass),
             "legacy fixed keyframe combination is not bypassed");

    auto invalid = fixedMixData();
    invalid.dynamicKeyframes = {
        {0, {0.1}}
    };
    QVERIFY2(!hasDynamicMixAutomation(invalid),
             "invalid inactive keyframes are not reported as automation");
}

void VoiceAndInferenceTests::speakerMixStaticAndFixedInferenceMix() {
    using namespace InferSpeakerMixModel;


    QVERIFY2(staticSpeakerMix("").isEmpty(), "empty static speaker mix is empty");

    const auto single = staticSpeakerMix("spk-a");
    QCOMPARE(single.fallbackSpeaker, "spk-a");
    QCOMPARE(single.sources.size(), 1);
    QCOMPARE(single.sources.first().speaker, "spk-a");
    if (!compareVectorNear(single.sources.first().proportions, {1.0}, __FILE__, __LINE__))
        return;

    SpeakerMixModel::SpeakerMixData singleMode;
    QCOMPARE(fixedSpeakerMixFromData(singleMode, "fallback"), staticSpeakerMix("fallback"));

    auto fixed = fixedMixData();
    fixed.fixedWeights = {0.2, 0.7};
    const auto mix = fixedSpeakerMixFromData(fixed, "fallback");
    QCOMPARE(mix.sources.size(), 3);
    QCOMPARE(mix.sources.at(0).speaker, "spk-a");
    QCOMPARE(mix.sources.at(1).speaker, "spk-b");
    QCOMPARE(mix.sources.at(2).speaker, "spk-c");
    if (!compareVectorNear(mix.sources.at(0).proportions, {0.2}, __FILE__, __LINE__))
        return;
    if (!compareVectorNear(mix.sources.at(1).proportions, {0.7}, __FILE__, __LINE__))
        return;
    if (!compareVectorNear(mix.sources.at(2).proportions, {0.1}, __FILE__, __LINE__))
        return;
    QCOMPARE(mix.fallbackSpeaker, "spk-b");

    auto invalid = fixedMixData();
    invalid.sources[0] = source("");
    QCOMPARE(fixedSpeakerMixFromData(invalid, "fallback"), staticSpeakerMix("fallback"));

    const InferSpeakerMix same = mix;
    auto changed = mix;
    changed.sources[0].proportions[0] = 0.25;
    QCOMPARE(mix.signature(), same.signature());
    QVERIFY2(mix.signature() != changed.signature(), "changed inference mix signature changes");
}

void VoiceAndInferenceTests::speakerMixDynamicInferenceMix() {
    using namespace InferSpeakerMixModel;


    const auto timeline = timeline120Bpm();
    const auto dynamic = dynamicMixData();
    const auto mix = dynamicSpeakerMixFromData(dynamic, "fallback", 0, 960, 0, timeline, 0.5);

    QCOMPARE(mix.sources.size(), 2);
    QCOMPARE(mix.sources.at(0).speaker, "spk-a");
    QCOMPARE(mix.sources.at(1).speaker, "spk-b");
    QVERIFY2(std::abs((mix.sources.at(0).interval) - (0.5)) <= kTolerance,
             "dynamic inference interval");
    if (!compareVectorNear(mix.sources.at(0).proportions, {0.0, 0.5}, __FILE__, __LINE__))
        return;
    if (!compareVectorNear(mix.sources.at(1).proportions, {1.0, 0.5}, __FILE__, __LINE__))
        return;
    QCOMPARE(mix.fallbackSpeaker, "spk-b");

    const auto fixedFallback = fixedSpeakerMixFromData(dynamic, "fallback");
    QCOMPARE(dynamicSpeakerMixFromData(dynamic, "fallback", 960, 0, 0, timeline, 0.5),
             fixedFallback);
    QCOMPARE(dynamicSpeakerMixFromData(dynamic, "fallback", 0, 960, 0, timeline, 0.0),
             fixedFallback);

    QCOMPARE(effectiveSpeakerMixFromData(dynamic, "fallback", 0, 960, 0, timeline, 0.5), mix);

    const auto shiftedMix =
        effectiveSpeakerMixFromData(dynamic, "fallback", 1920, 2880, 1920, timeline, 0.5);
    QCOMPARE(shiftedMix, mix);

    auto bypassed = dynamicMixData();
    bypassed.dynamicBypassed = true;
    bypassed.fixedWeights = {0.1};
    QCOMPARE(effectiveSpeakerMixFromData(bypassed, "fallback", 0, 960, 0, timeline, 0.5),
             fixedSpeakerMixFromData(bypassed, "fallback"));
}
