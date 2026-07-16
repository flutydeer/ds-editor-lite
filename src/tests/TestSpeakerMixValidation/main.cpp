// Regression tests for v3 spk validation (08-spk-validation.md).
// Covers:
//   - SpeakerInfo toneRange + mixable fields (Group A)
//   - SingerInfo.capability summary (Group B)
//   - SpeakerMixValidator.validate extreme-case matrix (Group D, §4 #1-10)
//
// Tests for PackageManager propagation (Group C) and DspxProjectConverter
// (Group E) are integration-level and require PackageManager/AppModel wiring;
// they are deferred to a future integration test target. The validator
// behavior is fully covered here by manually constructing SingerInfo +
// capability.

#include "Model/AppModel/InferPiece.h"
#include "Modules/Inference/Models/InferSpeakerMix.h"
#include "Modules/Inference/Models/SpeakerMixValidator.h"
#include "Modules/PackageManager/Models/SingerInfo.h"
#include "Modules/PackageManager/Models/SpeakerInfo.h"

#include <QCoreApplication>
#include <QTextStream>

#include <cmath>
#include <utility>

namespace {
    bool expect(const bool condition, const char *message) {
        if (condition)
            return true;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        return false;
    }

    SingerIdentifier makeIdentifier(const QString &singerId, const QString &pkgId = "pkg",
                                    const QString &version = "1.0.0") {
        return {singerId, pkgId, QVersionNumber::fromString(version)};
    }

    SpeakerInfo makeSpk(const QString &id, const QString &name = QString()) {
        return SpeakerInfo(id, name.isEmpty() ? id : name);
    }

    SingerInfo makeSingerWithCapability(const QStringList &speakerIds,
                                        const QStringList &mixableSpeakers,
                                        int speakerConsistency = 0 /*Ideal*/) {
        QList<SpeakerInfo> spks;
        for (const auto &id : speakerIds)
            spks.append(makeSpk(id));
        SingerInfo info(makeIdentifier("S1"), "Singer", spks, {}, "zh");
        info.setResolutionState(ResolutionState::Resolved);
        SingerCapabilitySummary cap;
        for (const auto &s : mixableSpeakers)
            cap.mixableSpeakers.append(s);
        cap.speakerConsistency = speakerConsistency;
        info.setCapability(cap);
        // Mark mixable flag on each SpeakerInfo for Group A test reuse
        auto spkList = info.speakers();
        QSet<QString> mixableSet(mixableSpeakers.begin(), mixableSpeakers.end());
        for (auto &s : spkList)
            s.setMixable(mixableSet.contains(s.id()));
        info.setSpeakers(spkList);
        return info;
    }

    InferSpeakerMix makeStaticMix(const QString &speaker) {
        return InferSpeakerMixModel::staticSpeakerMix(speaker);
    }

    InferSpeakerMix makeMix2(const QString &s1, double w1, const QString &s2, double w2) {
        InferSpeakerMix mix;
        mix.fallbackSpeaker = s1;
        InferSpeakerMixSource src1;
        src1.speaker = s1;
        src1.interval = 0;
        src1.proportions = {w1};
        InferSpeakerMixSource src2;
        src2.speaker = s2;
        src2.interval = 0;
        src2.proportions = {w2};
        mix.sources = {src1, src2};
        return mix;
    }

    // ---- Group A: SpeakerInfo toneRange + mixable ----

    bool testSpeakerInfoDefaults() {
        bool ok = true;
        SpeakerInfo s;
        ok &= expect(!s.toneRange().has_value(), "A1: default toneRange is nullopt");
        ok &= expect(s.mixable() == false, "A1: default mixable is false");
        return ok;
    }

    bool testSpeakerInfoToneRange() {
        bool ok = true;
        SpeakerInfo s("S1", "Speaker1");
        s.setToneRange(std::make_pair(48, 84));
        ok &= expect(s.toneRange().has_value(), "A2: toneRange set");
        ok &= expect(s.toneRange()->first == 48 && s.toneRange()->second == 84,
                     "A2: toneRange value {48,84}");
        // Backward compat: toneMin/toneMax QString fields not auto-synced here
        // (PackageManager does the sync). Just verify field independence.
        s.setToneMin("48");
        s.setToneMax("84");
        ok &= expect(s.toneMin() == "48" && s.toneMax() == "84",
                     "A2: toneMin/toneMax QString preserved");
        return ok;
    }

    bool testSpeakerInfoMixable() {
        bool ok = true;
        SpeakerInfo s("S1");
        s.setMixable(true);
        ok &= expect(s.mixable() == true, "A3: mixable set true");
        s.setMixable(false);
        ok &= expect(s.mixable() == false, "A3: mixable set false");
        return ok;
    }

    bool testSpeakerInfoEquality() {
        bool ok = true;
        SpeakerInfo a("S1", "Spk1");
        SpeakerInfo b("S1", "Spk1");
        ok &= expect(a == b, "A: equal SpeakerInfo");

        b.setToneRange(std::make_pair(48, 84));
        ok &= expect(a != b, "A: toneRange differs");

        b.setToneRange(std::nullopt);
        b.setMixable(true);
        ok &= expect(a != b, "A: mixable differs");
        return ok;
    }

    // ---- Group B: SingerInfo.capability summary ----

    bool testSingerInfoCapabilityDefault() {
        bool ok = true;
        SingerInfo info(makeIdentifier("S1"), "Singer");
        ok &= expect(!info.capability().has_value(), "B1: default capability is nullopt");
        return ok;
    }

    bool testSingerInfoCapabilitySet() {
        bool ok = true;
        SingerInfo info(makeIdentifier("S1"), "Singer");
        SingerCapabilitySummary cap;
        cap.mixableSpeakers = {"S1", "S2"};
        cap.speakerConsistency = 1; // Degraded
        cap.speakerWarnings = {"w1"};
        cap.effectivePhonemes = {"a", "e"};
        cap.phonemeConsistency = 0;
        cap.phonemeDegraded = true;
        cap.effectiveLanguages = {"zh"};
        cap.languageConsistency = 2;
        info.setCapability(cap);

        const auto &c = info.capability();
        ok &= expect(c.has_value(), "B2: capability has value");
        ok &= expect(c->mixableSpeakers == QStringList{"S1", "S2"},
                     "B2: mixableSpeakers populated");
        ok &= expect(c->speakerConsistency == 1, "B2: speakerConsistency Degraded");
        ok &= expect(c->speakerWarnings == QStringList{"w1"}, "B2: speakerWarnings");
        ok &= expect(c->effectivePhonemes == QStringList{"a", "e"}, "B2: effectivePhonemes");
        ok &= expect(c->phonemeDegraded == true, "B2: phonemeDegraded");
        ok &= expect(c->effectiveLanguages == QStringList{"zh"}, "B2: effectiveLanguages");
        ok &= expect(c->languageConsistency == 2, "B2: languageConsistency Inconsistent");
        return ok;
    }

    bool testConsistencyText() {
        bool ok = true;
        ok &= expect(SingerCapabilitySummary::consistencyText(0) == "Ideal", "B3: 0=Ideal");
        ok &= expect(SingerCapabilitySummary::consistencyText(1) == "Degraded", "B3: 1=Degraded");
        ok &= expect(SingerCapabilitySummary::consistencyText(2) == "Inconsistent",
                     "B3: 2=Inconsistent");
        ok &= expect(SingerCapabilitySummary::consistencyText(99) == "Unknown", "B3: invalid=Unknown");
        return ok;
    }

    // ---- Group D: SpeakerMixValidator.validate (§4 #1-10) ----

    // #1: single spk singer, speaker=S1, mix empty, mixable={S1}
    bool testValidateCase1_OkSingle() {
        bool ok = true;
        auto singer = makeSingerWithCapability({"S1"}, {"S1"});
        const auto result = SpeakerMixValidator::validate("S1", {}, singer);
        ok &= expect(result.status == SpeakerMixValidator::Status::Ok, "#1: status Ok");
        ok &= expect(result.primarySpeaker == "S1", "#1: primary S1");
        ok &= expect(!result.sanitizedMix.isEmpty(), "#1: sanitized non-empty");
        ok &= expect(result.droppedSpeakers.isEmpty(), "#1: no dropped");
        return ok;
    }

    // #2: all mixable, mix{S1:0.6, S2:0.4}
    bool testValidateCase2_OkMulti() {
        bool ok = true;
        auto singer = makeSingerWithCapability({"S1", "S2"}, {"S1", "S2"});
        const auto mix = makeMix2("S1", 0.6, "S2", 0.4);
        const auto result = SpeakerMixValidator::validate("S1", mix, singer);
        ok &= expect(result.status == SpeakerMixValidator::Status::Ok, "#2: status Ok");
        ok &= expect(result.primarySpeaker == "S1", "#2: primary S1");
        ok &= expect(result.sanitizedMix == mix, "#2: mix unchanged");
        ok &= expect(result.droppedSpeakers.isEmpty(), "#2: no dropped");
        return ok;
    }

    // #3: partial drop, mix{S1:0.6, S2:0.4}, mixable={S1} only (S2 not in)
    bool testValidateCase3_DegradedPartial() {
        bool ok = true;
        auto singer = makeSingerWithCapability({"S1", "S2"}, {"S1"}); // S2 not mixable
        const auto mix = makeMix2("S1", 0.6, "S2", 0.4);
        const auto result = SpeakerMixValidator::validate("S1", mix, singer);
        ok &= expect(result.status == SpeakerMixValidator::Status::Degraded, "#3: status Degraded");
        ok &= expect(result.primarySpeaker == "S1", "#3: primary S1");
        ok &= expect(result.droppedSpeakers == QStringList{"S2"}, "#3: dropped S2");
        ok &= expect(result.sanitizedMix.sources.size() == 1, "#3: one source after filter");
        ok &= expect(result.sanitizedMix.sources.at(0).speaker == "S1", "#3: source S1");
        ok &= expect(!result.warningMessage.isEmpty(), "#3: warning message");
        return ok;
    }

    // #4: all sources dropped, speaker=S1 ∈ mixable
    bool testValidateCase4_InvalidAllDropped() {
        bool ok = true;
        auto singer = makeSingerWithCapability({"S1", "S2", "S3"}, {"S1"});
        const auto mix = makeMix2("S2", 0.6, "S3", 0.4); // S2, S3 not in mixable
        const auto result = SpeakerMixValidator::validate("S1", mix, singer);
        ok &= expect(result.status == SpeakerMixValidator::Status::Invalid, "#4: status Invalid");
        ok &= expect(result.primarySpeaker == "S1", "#4: primary S1");
        ok &= expect(result.droppedSpeakers.contains("S2") && result.droppedSpeakers.contains("S3"),
                     "#4: dropped S2 and S3");
        ok &= expect(!result.sanitizedMix.isEmpty(), "#4: sanitized non-empty (static)");
        ok &= expect(result.sanitizedMix.sources.size() == 1, "#4: static mix has 1 source");
        ok &= expect(result.sanitizedMix.sources.at(0).speaker == "S1", "#4: static S1");
        return ok;
    }

    // #5: speaker not in mixable but mix has valid sources
    bool testValidateCase5_SpeakerDropped() {
        bool ok = true;
        auto singer = makeSingerWithCapability({"S1", "S2", "S9"}, {"S1", "S2"});
        const auto mix = makeMix2("S1", 0.6, "S2", 0.4);
        const auto result = SpeakerMixValidator::validate("S9", mix, singer);
        ok &= expect(result.status == SpeakerMixValidator::Status::Degraded, "#5: status Degraded");
        ok &= expect(result.droppedSpeakers.contains("S9"), "#5: dropped S9");
        // primary should be filled from mix (S1 or S2)
        ok &= expect(result.primarySpeaker == "S1" || result.primarySpeaker == "S2",
                     "#5: primary from mix");
        // mix sources remain valid (both S1, S2 in mixable)
        ok &= expect(result.sanitizedMix.sources.size() == 2, "#5: 2 sources kept");
        return ok;
    }

    // #6: speaker and mix sources all invalid
    bool testValidateCase6_AllInvalid() {
        bool ok = true;
        auto singer = makeSingerWithCapability({"S1", "S2"}, {"S1", "S2"});
        const auto mix = makeMix2("S8", 1.0, "S9", 0.0); // S8, S9 not in singer
        // Use S9 as primary to ensure it gets dropped
        const auto result = SpeakerMixValidator::validate("S9", mix, singer);
        ok &= expect(result.status == SpeakerMixValidator::Status::Invalid, "#6: status Invalid");
        // primary must fall back to singer.speakers().first() = S1
        ok &= expect(result.primarySpeaker == "S1", "#6: primary from singer.speakers");
        ok &= expect(!result.sanitizedMix.isEmpty(), "#6: sanitized non-empty");
        ok &= expect(result.sanitizedMix.sources.at(0).speaker == "S1", "#6: static S1");
        return ok;
    }

    // #7: mixableSpeakers empty (Inconsistent) — degrade to speakers list
    bool testValidateCase7_EmptyMixableInconsistent() {
        bool ok = true;
        // Inconsistent: capability present but mixableSpeakers empty
        SingerInfo info(makeIdentifier("S1"), "Singer", {makeSpk("S1")}, {}, "zh");
        info.setResolutionState(ResolutionState::Resolved);
        SingerCapabilitySummary cap;
        cap.speakerConsistency = 2; // Inconsistent
        info.setCapability(cap);
        const auto result = SpeakerMixValidator::validate("S1", {}, info);
        ok &= expect(result.status == SpeakerMixValidator::Status::Ok, "#7: status Ok (degraded)");
        ok &= expect(result.primarySpeaker == "S1", "#7: primary S1");
        ok &= expect(!result.sanitizedMix.isEmpty(), "#7: sanitized non-empty");
        return ok;
    }

    // #8: pure G2P singer (capability=nullopt)
    bool testValidateCase8_PureG2P() {
        bool ok = true;
        SingerInfo info(makeIdentifier("G2P1"), "G2P", {makeSpk("S1")}, {}, "zh");
        info.setResolutionState(ResolutionState::Resolved);
        // capability left as nullopt
        const auto result = SpeakerMixValidator::validate("S1", {}, info);
        ok &= expect(result.status == SpeakerMixValidator::Status::Ok, "#8: status Ok (conservative)");
        ok &= expect(result.primarySpeaker == "S1", "#8: primary S1");
        return ok;
    }

    // #9: legacy lite data (capability=nullopt, speakers empty)
    bool testValidateCase9_LegacyEmpty() {
        bool ok = true;
        SingerInfo info(makeIdentifier("S1"), "Legacy", {}, {}, "zh");
        info.setResolutionState(ResolutionState::Resolved);
        const auto result = SpeakerMixValidator::validate("S1", {}, info);
        ok &= expect(result.status == SpeakerMixValidator::Status::Ok,
                     "#9: status Ok (conservative, no speakers)");
        ok &= expect(result.primarySpeaker == "S1", "#9: primary S1 preserved");
        return ok;
    }

    // #10: empty speaker + empty mix + empty capability
    bool testValidateCase10_AllEmpty() {
        bool ok = true;
        SingerInfo info(makeIdentifier("S1"), "Empty", {}, {}, "zh");
        info.setResolutionState(ResolutionState::Resolved);
        const auto result = SpeakerMixValidator::validate({}, {}, info);
        ok &= expect(result.status == SpeakerMixValidator::Status::Ok, "#10: status Ok (conservative)");
        // primary empty, host UI should prompt user
        ok &= expect(result.primarySpeaker.isEmpty(), "#10: primary empty");
        return ok;
    }

    // #11: singer not Resolved (Pending) — conservative pass-through
    bool testValidateCase11_PendingSinger() {
        bool ok = true;
        SingerInfo info(makeIdentifier("S1"), "Pending", {makeSpk("S1")}, {}, "zh");
        info.setResolutionState(ResolutionState::Pending);
        const auto mix = makeMix2("S1", 0.6, "S2", 0.4);
        const auto result = SpeakerMixValidator::validate("S1", mix, info);
        ok &= expect(result.status == SpeakerMixValidator::Status::Ok,
                     "#11: status Ok (Pending pass-through)");
        ok &= expect(result.primarySpeaker == "S1", "#11: primary S1");
        ok &= expect(result.sanitizedMix == mix, "#11: mix unchanged");
        ok &= expect(result.droppedSpeakers.isEmpty(), "#11: no dropped");
        return ok;
    }

    // #12: Missing singer — same conservative pass-through
    bool testValidateCase12_MissingSinger() {
        bool ok = true;
        SingerInfo info(makeIdentifier("S1"), "Missing", {makeSpk("S1")}, {}, "zh");
        info.setResolutionState(ResolutionState::Missing);
        const auto result = SpeakerMixValidator::validate("S1", {}, info);
        ok &= expect(result.status == SpeakerMixValidator::Status::Ok,
                     "#12: status Ok (Missing pass-through)");
        ok &= expect(result.primarySpeaker == "S1", "#12: primary S1");
        return ok;
    }

    // #13: partial drop with multi-frame proportions, verify renormalization
    bool testValidateCase13_RenormalizeMultiFrame() {
        bool ok = true;
        auto singer = makeSingerWithCapability({"S1", "S2", "S3"}, {"S1", "S2"}); // S3 not mixable
        InferSpeakerMix mix;
        mix.fallbackSpeaker = "S1";
        InferSpeakerMixSource src1;
        src1.speaker = "S1";
        src1.interval = 0.01;
        src1.proportions = {0.6, 0.5, 0.4};
        InferSpeakerMixSource src2;
        src2.speaker = "S2";
        src2.interval = 0.01;
        src2.proportions = {0.3, 0.4, 0.5};
        InferSpeakerMixSource src3;
        src3.speaker = "S3"; // not mixable
        src3.interval = 0.01;
        src3.proportions = {0.1, 0.1, 0.1};
        mix.sources = {src1, src2, src3};

        const auto result = SpeakerMixValidator::validate("S1", mix, singer);
        ok &= expect(result.status == SpeakerMixValidator::Status::Degraded,
                     "#13: status Degraded");
        ok &= expect(result.droppedSpeakers == QStringList{"S3"}, "#13: dropped S3");
        ok &= expect(result.sanitizedMix.sources.size() == 2, "#13: 2 sources after filter");
        // After renormalization, frame 0: 0.6/(0.6+0.3) = 0.667, 0.3/0.9 = 0.333
        const auto &p1 = result.sanitizedMix.sources.at(0).proportions;
        const auto &p2 = result.sanitizedMix.sources.at(1).proportions;
        ok &= expect(p1.size() == 3 && p2.size() == 3, "#13: 3 frames preserved");
        ok &= expect(std::abs(p1[0] - 0.6667) < 0.01, "#13: frame0 S1 renormalized");
        ok &= expect(std::abs(p2[0] - 0.3333) < 0.01, "#13: frame0 S2 renormalized");
        // Frame sum should be ~1.0 after renormalization
        ok &= expect(std::abs(p1[1] + p2[1] - 1.0) < 0.001, "#13: frame1 sum=1.0");
        return ok;
    }

    // #14: fallbackSpeaker dropped but sources valid — fallback should be
    // repicked from surviving sources
    bool testValidateCase14_FallbackRepicked() {
        bool ok = true;
        auto singer = makeSingerWithCapability({"S1", "S2", "S3"}, {"S1", "S2"}); // S3 not mixable
        InferSpeakerMix mix;
        mix.fallbackSpeaker = "S3"; // S3 not in mixable, must be repicked
        InferSpeakerMixSource src1;
        src1.speaker = "S1";
        src1.interval = 0;
        src1.proportions = {0.6};
        InferSpeakerMixSource src2;
        src2.speaker = "S2";
        src2.interval = 0;
        src2.proportions = {0.4};
        mix.sources = {src1, src2};

        const auto result = SpeakerMixValidator::validate("S1", mix, singer);
        ok &= expect(result.droppedSpeakers.contains("S3"), "#14: dropped S3 fallback");
        ok &= expect(result.sanitizedMix.fallbackSpeaker == "S1",
                     "#14: fallback repicked to S1 (preferred by primary)");
        ok &= expect(result.sanitizedMix.sources.size() == 2, "#14: 2 sources kept");
        return ok;
    }
} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    int failed = 0;
    auto run = [&](bool (*test)(), const char *name) {
        if (!test()) {
            ++failed;
            QTextStream(stderr) << "  in: " << name << Qt::endl;
        }
    };

    run(testSpeakerInfoDefaults, "testSpeakerInfoDefaults");
    run(testSpeakerInfoToneRange, "testSpeakerInfoToneRange");
    run(testSpeakerInfoMixable, "testSpeakerInfoMixable");
    run(testSpeakerInfoEquality, "testSpeakerInfoEquality");

    run(testSingerInfoCapabilityDefault, "testSingerInfoCapabilityDefault");
    run(testSingerInfoCapabilitySet, "testSingerInfoCapabilitySet");
    run(testConsistencyText, "testConsistencyText");

    run(testValidateCase1_OkSingle, "testValidateCase1_OkSingle");
    run(testValidateCase2_OkMulti, "testValidateCase2_OkMulti");
    run(testValidateCase3_DegradedPartial, "testValidateCase3_DegradedPartial");
    run(testValidateCase4_InvalidAllDropped, "testValidateCase4_InvalidAllDropped");
    run(testValidateCase5_SpeakerDropped, "testValidateCase5_SpeakerDropped");
    run(testValidateCase6_AllInvalid, "testValidateCase6_AllInvalid");
    run(testValidateCase7_EmptyMixableInconsistent, "testValidateCase7_EmptyMixableInconsistent");
    run(testValidateCase8_PureG2P, "testValidateCase8_PureG2P");
    run(testValidateCase9_LegacyEmpty, "testValidateCase9_LegacyEmpty");
    run(testValidateCase10_AllEmpty, "testValidateCase10_AllEmpty");
    run(testValidateCase11_PendingSinger, "testValidateCase11_PendingSinger");
    run(testValidateCase12_MissingSinger, "testValidateCase12_MissingSinger");
    run(testValidateCase13_RenormalizeMultiFrame, "testValidateCase13_RenormalizeMultiFrame");
    run(testValidateCase14_FallbackRepicked, "testValidateCase14_FallbackRepicked");

    if (failed == 0) {
        QTextStream(stderr) << "All tests passed." << Qt::endl;
        return 0;
    }
    QTextStream(stderr) << failed << " test(s) failed." << Qt::endl;
    return 1;
}
