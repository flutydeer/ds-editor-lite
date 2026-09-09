#include "InputConversionTests.h"

#include "Modules/Inference/Models/SpeakerMixValidator.h"

#include <cmath>

namespace {
    SingerInfo singerWithSpeakers(const QStringList &speakers, const QStringList &mixable,
                                  const bool hasCapability = true) {
        QList<SpeakerInfo> entries;
        for (const auto &id : speakers)
            entries.append(SpeakerInfo(id));
        SingerInfo singer({"singer", "package", QVersionNumber(1, 0)}, "Singer", entries);
        singer.setResolutionState(ResolutionState::Resolved);
        if (hasCapability) {
            SingerCapabilitySummary capability;
            capability.mixableSpeakers = mixable;
            singer.setCapability(capability);
        }
        return singer;
    }
}

void InputConversionTests::availableSpeakersFillEmptyMix_data() {
    QTest::addColumn<bool>("hasCapability");
    QTest::addColumn<QStringList>("mixable");
    QTest::newRow("known-capability") << true << QStringList{"S1"};
    QTest::newRow("empty-capability-falls-back-to-speaker-list") << true << QStringList{};
    QTest::newRow("absent-capability-falls-back-to-speaker-list") << false << QStringList{};
}

void InputConversionTests::availableSpeakersFillEmptyMix() {
    QFETCH(bool, hasCapability);
    QFETCH(QStringList, mixable);
    const auto result =
        SpeakerMixValidator::validate("S1", {}, singerWithSpeakers({"S1"}, mixable, hasCapability));
    QCOMPARE(result.status, SpeakerMixValidator::Status::Ok);
    QCOMPARE(result.primarySpeaker, QStringLiteral("S1"));
    QVERIFY(result.sanitizedMix == InferSpeakerMixModel::staticSpeakerMix("S1"));
    QVERIFY(result.droppedSpeakers.isEmpty());
}

void InputConversionTests::availableMixIsPreserved() {
    const auto mix = twoSpeakerMix();
    const auto result =
        SpeakerMixValidator::validate("S1", mix, singerWithSpeakers({"S1", "S2"}, {"S1", "S2"}));
    QCOMPARE(result.status, SpeakerMixValidator::Status::Ok);
    QCOMPARE(result.primarySpeaker, QStringLiteral("S1"));
    QVERIFY(result.sanitizedMix == mix);
    QVERIFY(result.droppedSpeakers.isEmpty());
}

void InputConversionTests::unavailableSourcesAreRemoved() {
    const auto result = SpeakerMixValidator::validate("S1", twoSpeakerMix(),
                                                      singerWithSpeakers({"S1", "S2"}, {"S1"}));
    QCOMPARE(result.status, SpeakerMixValidator::Status::Degraded);
    QCOMPARE(result.primarySpeaker, QStringLiteral("S1"));
    QCOMPARE(result.droppedSpeakers, QStringList{"S2"});
    QVERIFY(result.sanitizedMix == InferSpeakerMixModel::staticSpeakerMix("S1"));
    QVERIFY(!result.warningMessage.isEmpty());
}

void InputConversionTests::unusableMixFallsBackToAvailableSpeaker_data() {
    QTest::addColumn<QString>("primary");
    QTest::newRow("valid-primary") << QStringLiteral("S1");
    QTest::newRow("invalid-primary") << QStringLiteral("S9");
}

void InputConversionTests::unusableMixFallsBackToAvailableSpeaker() {
    QFETCH(QString, primary);
    const auto result = SpeakerMixValidator::validate(primary, twoSpeakerMix("S8", "S9"),
                                                      singerWithSpeakers({"S1", "S2"}, {"S1"}));
    QCOMPARE(result.status, SpeakerMixValidator::Status::Invalid);
    QCOMPARE(result.primarySpeaker, QStringLiteral("S1"));
    QVERIFY(result.droppedSpeakers.contains(QStringLiteral("S8")));
    QVERIFY(result.droppedSpeakers.contains(QStringLiteral("S9")));
    QVERIFY(result.sanitizedMix == InferSpeakerMixModel::staticSpeakerMix("S1"));
}

void InputConversionTests::unavailablePrimaryIsReplaced() {
    const auto mix = twoSpeakerMix();
    const auto result = SpeakerMixValidator::validate(
        "S9", mix, singerWithSpeakers({"S1", "S2", "S9"}, {"S1", "S2"}));
    QCOMPARE(result.status, SpeakerMixValidator::Status::Degraded);
    QCOMPARE(result.primarySpeaker, QStringLiteral("S1"));
    QCOMPARE(result.droppedSpeakers, QStringList{"S9"});
    QVERIFY(result.sanitizedMix == mix);
}

void InputConversionTests::unavailableFallbackIsReplaced() {
    auto mix = twoSpeakerMix();
    mix.fallbackSpeaker = QStringLiteral("S3");
    const auto result = SpeakerMixValidator::validate(
        "S1", mix, singerWithSpeakers({"S1", "S2", "S3"}, {"S1", "S2"}));
    QCOMPARE(result.status, SpeakerMixValidator::Status::Degraded);
    QCOMPARE(result.droppedSpeakers, QStringList{"S3"});
    QCOMPARE(result.sanitizedMix.fallbackSpeaker, QStringLiteral("S1"));
    QCOMPARE(result.sanitizedMix.sources, mix.sources);
}

void InputConversionTests::unresolvedSpeakerMetadataPreservesInput_data() {
    QTest::addColumn<int>("resolution");
    QTest::addColumn<QString>("primary");
    QTest::addColumn<bool>("hasMix");
    QTest::newRow("legacy-speaker")
        << int(ResolutionState::Resolved) << QStringLiteral("S1") << false;
    QTest::newRow("legacy-empty") << int(ResolutionState::Resolved) << QString{} << false;
    QTest::newRow("pending-singer")
        << int(ResolutionState::Pending) << QStringLiteral("S1") << true;
    QTest::newRow("missing-singer")
        << int(ResolutionState::Missing) << QStringLiteral("S1") << false;
}

void InputConversionTests::unresolvedSpeakerMetadataPreservesInput() {
    QFETCH(int, resolution);
    QFETCH(QString, primary);
    QFETCH(bool, hasMix);
    auto singer = singerWithSpeakers({}, {}, false);
    singer.setResolutionState(static_cast<ResolutionState>(resolution));
    const auto mix = hasMix ? twoSpeakerMix() : InferSpeakerMix{};
    const auto result = SpeakerMixValidator::validate(primary, mix, singer);
    QCOMPARE(result.status, SpeakerMixValidator::Status::Ok);
    QCOMPARE(result.primarySpeaker, primary);
    QVERIFY(result.sanitizedMix == mix);
    QVERIFY(result.droppedSpeakers.isEmpty());
}

void InputConversionTests::survivingMixCurvesAreRenormalized() {
    InferSpeakerMix mix;
    mix.fallbackSpeaker = QStringLiteral("S1");
    mix.sources = {
        {"S1", 0.01, {0.6, 0.5, 0.4}},
        {"S2", 0.01, {0.3, 0.4, 0.5}},
        {"S3", 0.01, {0.1, 0.1, 0.1}}
    };
    const auto result = SpeakerMixValidator::validate(
        "S1", mix, singerWithSpeakers({"S1", "S2", "S3"}, {"S1", "S2"}));
    QCOMPARE(result.status, SpeakerMixValidator::Status::Degraded);
    QCOMPARE(result.droppedSpeakers, QStringList{"S3"});
    QCOMPARE(result.sanitizedMix.sources.size(), 2);
    const auto &first = result.sanitizedMix.sources[0];
    const auto &second = result.sanitizedMix.sources[1];
    QCOMPARE(first.speaker, QStringLiteral("S1"));
    QCOMPARE(second.speaker, QStringLiteral("S2"));
    QCOMPARE(first.interval, 0.01);
    QCOMPARE(second.interval, 0.01);
    QCOMPARE(first.proportions.size(), 3);
    QCOMPARE(second.proportions.size(), 3);
    QVERIFY(std::abs(first.proportions[0] - 2.0 / 3.0) < 1e-9);
    QVERIFY(std::abs(first.proportions[1] - 5.0 / 9.0) < 1e-9);
    QVERIFY(std::abs(first.proportions[2] - 4.0 / 9.0) < 1e-9);
    for (qsizetype index = 0; index < first.proportions.size(); ++index)
        QVERIFY(std::abs(first.proportions[index] + second.proportions[index] - 1.0) < 1e-9);
}
