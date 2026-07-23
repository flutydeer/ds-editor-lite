#include "Model/AppModel/SpeakerMixData.h"
#include "Model/AppModel/Timeline.h"
#include "Model/InferenceData/InferSpeakerMix.h"

#include <QCoreApplication>
#include <QTextStream>

#include <cmath>

namespace {
    constexpr double kTolerance = 1e-9;

    bool expect(const bool condition, const char *message) {
        if (condition)
            return true;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        return false;
    }

    bool expectNear(const double actual, const double expected, const char *message) {
        return expect(std::abs(actual - expected) <= kTolerance, message);
    }

    bool expectVectorNear(const QVector<double> &actual, const QVector<double> &expected,
                          const char *message) {
        if (!expect(actual.size() == expected.size(), message))
            return false;
        for (int i = 0; i < actual.size(); ++i) {
            if (std::abs(actual.at(i) - expected.at(i)) > kTolerance) {
                QTextStream(stderr) << "FAILED: " << message << " at index " << i << " actual "
                                    << actual.at(i) << " expected " << expected.at(i) << Qt::endl;
                return false;
            }
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
        Timeline timeline;
        Tempo tempo;
        tempo.value = 120;
        timeline.tempos = {tempo};
        timeline.timeSignatures = {TimeSignature()};
        return timeline;
    }

    bool testWeightConversions() {
        using namespace SpeakerMixModel;

        bool ok = true;
        ok &= expectVectorNear(normalizeSpeakerMixFullWeights({-1.0, 2.0, 1.0}, 3), {0.0, 0.5, 0.5},
                               "full weights clamp and normalize");
        ok &= expectVectorNear(normalizeSpeakerMixFullWeights({0.0, 0.0, 0.0}, 3),
                               {1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0},
                               "zero full weights become equal weights");
        ok &= expect(normalizeSpeakerMixFullWeights({1.0}, 0).isEmpty(),
                     "non-positive source count returns empty weights");
        ok &= expectVectorNear(explicitWeightsFromFullWeights({2.0, 1.0, 1.0}),
                               {1.0 / 3.0, 1.0 / 3.0},
                               "explicit weights drop normalized implicit last source");
        ok &= expectVectorNear(fullWeightsFromExplicitWeights({0.2, 0.3}), {0.2, 0.3, 0.5},
                               "full weights append implicit remainder");
        ok &= expectVectorNear(fullWeightsFromExplicitWeights({0.8, 0.8}), {0.5, 0.5, 0.0},
                               "overflow explicit weights normalize with zero remainder");
        return ok;
    }

    bool testNormalizeSpeakerMixData() {
        using namespace SpeakerMixModel;

        bool ok = true;

        auto fixed = fixedMixData();
        fixed.fixedWeights = {2.0, 1.0};
        fixed.dynamicKeyframes = {
            {240, {2.0, 1.0}}
        };
        fixed.sourcePresetId = " preset ";
        fixed.sourcePresetName = " name ";
        const auto normalizedFixed = normalizeSpeakerMixData(fixed);
        ok &= expect(normalizedFixed.mode == SingerSourceMode::FixedMix,
                     "valid fixed mix remains fixed");
        ok &= expectVectorNear(normalizedFixed.fixedWeights, {0.5, 0.5}, "fixed weights normalize");
        ok &= expect(normalizedFixed.dynamicKeyframes.isEmpty(),
                     "fixed mix does not carry dynamic automation");
        ok &= expect(normalizedFixed.sourcePresetId == "preset" &&
                         normalizedFixed.sourcePresetName == "name",
                     "preset metadata is trimmed");

        auto noPreset = fixedMixData();
        noPreset.sourcePresetName = "stale";
        noPreset.sourcePresetDirty = true;
        const auto normalizedNoPreset = normalizeSpeakerMixData(noPreset);
        ok &= expect(normalizedNoPreset.sourcePresetName.isEmpty() &&
                         !normalizedNoPreset.sourcePresetDirty,
                     "preset name and dirty flag clear when preset id is empty");

        auto invalidFixed = fixedMixData();
        invalidFixed.fixedWeights = {0.5};
        ok &= expect(normalizeSpeakerMixData(invalidFixed).mode == SingerSourceMode::Single,
                     "fixed mix with wrong weight count falls back to single");

        auto emptySource = fixedMixData();
        emptySource.sources[1] = source("");
        ok &= expect(normalizeSpeakerMixData(emptySource).mode == SingerSourceMode::Single,
                     "mix with empty speaker falls back to single");

        auto dynamic = dynamicMixData();
        const auto normalizedDynamic = normalizeSpeakerMixData(dynamic);
        ok &= expect(normalizedDynamic.mode == SingerSourceMode::DynamicMix,
                     "valid dynamic mix remains dynamic");
        ok &= expect(normalizedDynamic.dynamicKeyframes.first().tick == 0 &&
                         normalizedDynamic.dynamicKeyframes.last().tick == 960,
                     "dynamic keyframes are sorted");

        auto invalidDynamic = dynamicMixData();
        invalidDynamic.dynamicKeyframes.first().weights = {0.2, 0.3};
        ok &= expect(normalizeSpeakerMixData(invalidDynamic).mode == SingerSourceMode::Single,
                     "dynamic mix with wrong keyframe weight count falls back to single");

        return ok;
    }

    bool testDynamicStatePredicates() {
        using namespace SpeakerMixModel;

        bool ok = true;

        const auto dynamic = dynamicMixData();
        ok &= expect(hasDynamicMixAutomation(dynamic), "dynamic mix reports automation");
        ok &= expect(isDynamicMixActive(dynamic), "dynamic mix reports active");
        ok &= expect(!isDynamicMixBypassed(dynamic), "dynamic mix is not bypassed");

        auto bypassed = dynamicMixData();
        bypassed.dynamicBypassed = true;
        bypassed.fixedWeights = {0.1};
        ok &= expect(hasDynamicMixAutomation(bypassed), "bypassed dynamic mix reports automation");
        ok &= expect(!isDynamicMixActive(bypassed), "bypassed dynamic mix is not active");
        ok &= expect(isDynamicMixBypassed(bypassed), "bypassed dynamic mix reports bypassed");

        auto bypassedWithoutFixedWeights = dynamicMixData();
        bypassedWithoutFixedWeights.dynamicBypassed = true;
        ok &= expectVectorNear(normalizeSpeakerMixData(bypassedWithoutFixedWeights).fixedWeights,
                               {0.0},
                               "bypassed dynamic mix gets fixed weights from first keyframe");

        auto legacyBypass = fixedMixData();
        legacyBypass.dynamicKeyframes = {
            {0, {0.1, 0.2}}
        };
        ok &= expect(!hasDynamicMixAutomation(legacyBypass),
                     "fixed mix with keyframes no longer means bypassed dynamic mix");
        ok &= expect(!isDynamicMixBypassed(legacyBypass),
                     "legacy fixed keyframe combination is not bypassed");

        auto invalid = fixedMixData();
        invalid.dynamicKeyframes = {
            {0, {0.1}}
        };
        ok &= expect(!hasDynamicMixAutomation(invalid),
                     "invalid inactive keyframes are not reported as automation");

        return ok;
    }

    bool testStaticAndFixedInferenceMix() {
        using namespace InferSpeakerMixModel;

        bool ok = true;

        ok &= expect(staticSpeakerMix("").isEmpty(), "empty static speaker mix is empty");

        const auto single = staticSpeakerMix("spk-a");
        ok &= expect(single.fallbackSpeaker == "spk-a", "static mix fallback is speaker");
        ok &= expect(single.sources.size() == 1, "static mix has one source");
        ok &= expect(single.sources.first().speaker == "spk-a", "static mix source speaker");
        ok &=
            expectVectorNear(single.sources.first().proportions, {1.0}, "static mix source weight");

        SpeakerMixModel::SpeakerMixData singleMode;
        ok &=
            expect(fixedSpeakerMixFromData(singleMode, "fallback") == staticSpeakerMix("fallback"),
                   "single mode fixed inference uses fallback");

        auto fixed = fixedMixData();
        fixed.fixedWeights = {0.2, 0.7};
        const auto mix = fixedSpeakerMixFromData(fixed, "fallback");
        ok &= expect(mix.sources.size() == 3, "fixed inference keeps all sources");
        ok &= expect(mix.sources.at(0).speaker == "spk-a" && mix.sources.at(1).speaker == "spk-b" &&
                         mix.sources.at(2).speaker == "spk-c",
                     "fixed inference maps speaker ids");
        ok &= expectVectorNear(mix.sources.at(0).proportions, {0.2},
                               "fixed inference first source weight");
        ok &= expectVectorNear(mix.sources.at(1).proportions, {0.7},
                               "fixed inference second source weight");
        ok &= expectVectorNear(mix.sources.at(2).proportions, {0.1},
                               "fixed inference implicit source weight");
        ok &= expect(mix.fallbackSpeaker == "spk-b",
                     "fixed inference fallback chooses highest average weight");

        auto invalid = fixedMixData();
        invalid.sources[0] = source("");
        ok &= expect(fixedSpeakerMixFromData(invalid, "fallback") == staticSpeakerMix("fallback"),
                     "invalid fixed inference falls back to static speaker");

        const InferSpeakerMix same = mix;
        auto changed = mix;
        changed.sources[0].proportions[0] = 0.25;
        ok &= expect(mix.signature() == same.signature(), "same inference mix signature is stable");
        ok &= expect(mix.signature() != changed.signature(),
                     "changed inference mix signature changes");

        return ok;
    }

    bool testDynamicInferenceMix() {
        using namespace InferSpeakerMixModel;

        bool ok = true;

        const auto timeline = timeline120Bpm();
        const auto dynamic = dynamicMixData();
        const auto mix = dynamicSpeakerMixFromData(dynamic, "fallback", 0, 960, timeline, 0.5);

        ok &= expect(mix.sources.size() == 2, "dynamic inference keeps two sources");
        ok &= expect(mix.sources.at(0).speaker == "spk-a" && mix.sources.at(1).speaker == "spk-b",
                     "dynamic inference maps speaker ids");
        ok &= expectNear(mix.sources.at(0).interval, 0.5, "dynamic inference interval");
        ok &= expectVectorNear(mix.sources.at(0).proportions, {0.0, 0.5},
                               "dynamic inference interpolates first source");
        ok &= expectVectorNear(mix.sources.at(1).proportions, {1.0, 0.5},
                               "dynamic inference interpolates implicit source");
        ok &= expect(mix.fallbackSpeaker == "spk-b",
                     "dynamic inference fallback chooses highest average source");

        const auto fixedFallback = fixedSpeakerMixFromData(dynamic, "fallback");
        ok &= expect(dynamicSpeakerMixFromData(dynamic, "fallback", 960, 0, timeline, 0.5) ==
                         fixedFallback,
                     "dynamic inference with invalid range falls back to fixed");
        ok &= expect(dynamicSpeakerMixFromData(dynamic, "fallback", 0, 960, timeline, 0.0) ==
                         fixedFallback,
                     "dynamic inference with invalid interval falls back to fixed");

        ok &= expect(effectiveSpeakerMixFromData(dynamic, "fallback", 0, 960, timeline, 0.5) == mix,
                     "effective inference uses dynamic mix when active");

        auto bypassed = dynamicMixData();
        bypassed.dynamicBypassed = true;
        bypassed.fixedWeights = {0.1};
        ok &= expect(effectiveSpeakerMixFromData(bypassed, "fallback", 0, 960, timeline, 0.5) ==
                         fixedSpeakerMixFromData(bypassed, "fallback"),
                     "effective inference uses fixed mix when dynamic is bypassed");

        return ok;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    bool ok = true;
    ok &= testWeightConversions();
    ok &= testNormalizeSpeakerMixData();
    ok &= testDynamicStatePredicates();
    ok &= testStaticAndFixedInferenceMix();
    ok &= testDynamicInferenceMix();

    if (!ok)
        return 1;

    QTextStream(stdout) << "TestSpeakerMix passed" << Qt::endl;
    return 0;
}
