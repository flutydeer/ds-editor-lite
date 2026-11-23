#include "tst_voice_and_inference.h"

#include <lite/ProjectModel/Voice/LanguageCompatibility.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/EffectiveVoiceContext.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

#include <QtTest/QTest>

void VoiceAndInferenceTests::languageCompatibilityRoutesAndConflicts() {
    const auto singer = [](const QStringList &declared) {
        QList<LanguageInfo> languages;
        for (const auto &id : declared)
            languages.emplace_back(id, id, QStringLiteral("g2p"));
        SingerInfo info({QStringLiteral("singer"), QStringLiteral("package"), QVersionNumber(1, 0)},
                        QStringLiteral("Singer"), {}, languages, declared.value(0));
        info.setResolutionState(ResolutionState::Resolved);
        return info;
    };

    // Without a projection the declared languages are the only candidates, which
    // keeps documents and fixtures from older builds working.
    const auto declaredOnly = singer({QStringLiteral("cmn"), QStringLiteral("eng")});
    QVERIFY(!declaredOnly.hasConversionLanguages());
    auto support = LanguageCompatibility::conversionSupport(declaredOnly);
    QVERIFY(support.isKnown());
    QCOMPARE(support.languageIds, QStringList({QStringLiteral("cmn"), QStringLiteral("eng")}));
    QCOMPARE(LanguageCompatibility::fallbackLanguage(declaredOnly), QStringLiteral("cmn"));

    // A populated projection is authoritative: a declared language without a
    // route is not convertible and the conflict check reports it.
    auto routed = singer({QStringLiteral("cmn"), QStringLiteral("eng")});
    routed.setConversionLanguages({QStringLiteral("cmn")});
    support = LanguageCompatibility::conversionSupport(routed);
    QVERIFY(support.isKnown());
    QCOMPARE(support.languageIds, QStringList({QStringLiteral("cmn")}));
    QCOMPARE(LanguageCompatibility::fallbackLanguage(routed), QStringLiteral("cmn"));

    Note mandarin;
    mandarin.setLanguage(QStringLiteral("cmn"));
    Note english;
    english.setLanguage(QStringLiteral("eng"));
    Note unspecified;
    const QList<Note *> notes{&mandarin, &english, &unspecified};
    const auto conflicts = LanguageCompatibility::conflicts(notes, routed);
    QCOMPARE(conflicts.size(), 1);
    QCOMPARE(conflicts.first().language, QStringLiteral("eng"));
    QCOMPARE(conflicts.first().noteIds, QList<int>{english.id()});

    // An empty projection means every declared language lacks a route, so the
    // singer stays resolvable but cannot convert anything.
    auto unroutable = singer({QStringLiteral("cmn")});
    unroutable.setConversionLanguages({});
    support = LanguageCompatibility::conversionSupport(unroutable);
    QVERIFY(support.isKnown());
    QVERIFY(support.languageIds.isEmpty());
    QVERIFY(LanguageCompatibility::fallbackLanguage(unroutable).isEmpty());
    QCOMPARE(LanguageCompatibility::conflicts({&mandarin}, unroutable).size(), 1);

    // Unresolved metadata must not produce diagnostics.
    auto pending = singer({QStringLiteral("cmn")});
    pending.setResolutionState(ResolutionState::Pending);
    QVERIFY(!LanguageCompatibility::conversionSupport(pending).isKnown());
    QVERIFY(LanguageCompatibility::conflicts({&mandarin}, pending).isEmpty());
}
void VoiceAndInferenceTests::voiceChangeClearsStaleInferenceErrors() {
    // Missing phonemes make the slicer exclude the phrase, which publishes both a
    // per-note error and a skipped range on the clip.
    auto *silent = new Note;
    silent->setLocalStart(0);
    silent->setLength(480);
    silent->setLyric(QStringLiteral("la"));
    SingingClip clip({silent});
    clip.setTrackSingerAndSpeakerInfo(
        SingerInfo({QStringLiteral("first"), QStringLiteral("package"), QVersionNumber(1, 0)},
                   QStringLiteral("First")),
        SpeakerInfo(QStringLiteral("clear")));
    clip.reSegment(Timeline{});
    QVERIFY(clip.noteInferenceErrors().contains(silent->id()));
    QVERIFY(!clip.skippedPhraseRanges().isEmpty());

    // A singer change invalidates the previous inference input, so the transient
    // errors of that input must not survive it.
    clip.selectOwnSingleSpeaker(
        SingerInfo({QStringLiteral("second"), QStringLiteral("package"), QVersionNumber(1, 0)},
                   QStringLiteral("Second")),
        SpeakerInfo(QStringLiteral("clear")));
    QVERIFY(clip.noteInferenceErrors().isEmpty());
    QVERIFY(clip.skippedPhraseRanges().isEmpty());
}

void VoiceAndInferenceTests::knownEmptyConversionRoutesAreReported() {
    auto *unspecified = new Note;
    unspecified->setLocalStart(0);
    unspecified->setLength(480);
    unspecified->setLyric(QStringLiteral("la"));
    auto *declared = new Note;
    declared->setLocalStart(480);
    declared->setLength(480);
    declared->setLyric(QStringLiteral("la"));
    declared->setLanguage(QStringLiteral("cmn"));
    SingingClip clip({unspecified, declared});

    auto singer = SingerInfo(
        {QStringLiteral("mute"), QStringLiteral("package"), QVersionNumber(1, 0)},
        QStringLiteral("Mute"), {},
        {LanguageInfo(QStringLiteral("cmn"), QStringLiteral("Chinese"), QStringLiteral("g2p"))},
        QStringLiteral("cmn"));
    singer.setResolutionState(ResolutionState::Resolved);
    // A known and populated projection says no declared language has a route.
    singer.setConversionLanguages({});
    clip.setTrackSingerAndSpeakerInfo(singer, SpeakerInfo(QStringLiteral("clear")));

    const auto &errors = clip.noteInferenceErrors();
    QVERIFY(errors.contains(unspecified->id()));
    QCOMPARE(errors.value(unspecified->id()).reason,
             NoteInferenceIssueReason::UnsupportedLanguage);
    QVERIFY(errors.value(unspecified->id()).availableLanguages.isEmpty());
    // The declared note keeps the declaration conflict instead.
    QVERIFY(errors.contains(declared->id()));
    QCOMPARE(errors.value(declared->id()).language, QStringLiteral("cmn"));
}
void VoiceAndInferenceTests::voiceContextNotificationsContainAtomicBeforeAndAfter() {
    Track track;
    int notificationCount = 0;
    VoiceContextChange received;
    QObject::connect(&track, &Track::voiceContextChanged, [&](const VoiceContextChange &change) {
        ++notificationCount;
        received = change;
    });
    const SingerInfo singer({"singer", "package", QVersionNumber(1, 0)}, "Test Singer");
    const SpeakerInfo speaker("speaker");
    SpeakerMixModel::SpeakerMixData mix;
    mix.mode = SpeakerMixModel::SingerSourceMode::FixedMix;
    mix.sources = {{SpeakerInfo("speaker")}, {SpeakerInfo("speaker-b")}};
    mix.fixedWeights = {0.4};

    const auto before = track.voiceContext();
    track.setVoiceContext(singer, speaker, mix);
    const auto after = track.voiceContext();
    QCOMPARE(notificationCount, 1);
    QVERIFY(received.before == before);
    QVERIFY(received.after == after);

    track.setVoiceContext(singer, speaker, mix);
    QCOMPARE(notificationCount, 1);

    mix.fixedWeights = {0.7};
    track.setSpeakerMixData(mix);
    QCOMPARE(notificationCount, 2);
    QVERIFY(received.before == after);
    QVERIFY(received.after == track.voiceContext());
}

void VoiceAndInferenceTests::voiceContextInheritanceOnlyChangePreservesInferenceInput() {
    Track track;
    const auto context = track.voiceContext();
    auto inherited = context;
    inherited.followsTrack = !context.followsTrack;
    QVERIFY(context.hasSameInferenceInput(inherited));
}

void VoiceAndInferenceTests::voiceContextInheritedClipsFollowTrackWhileOwnVoiceRemains() {
    Track track;
    const SingerInfo singer({"singer", "package", QVersionNumber(1, 0)}, "Singer");
    track.setVoiceContext(singer, SpeakerInfo("S1"), {});
    auto *inherited = new SingingClip;
    auto *independent = new SingingClip;
    independent->setOwnVoiceContext(singer, SpeakerInfo("own"), {});
    track.insertClip(inherited);
    track.insertClip(independent);
    QVERIFY(inherited->usesTrackVoiceContext());
    QVERIFY(!independent->usesTrackVoiceContext());
    QCOMPARE(inherited->speakerId(), QStringLiteral("S1"));
    const auto ownContext = independent->effectiveVoiceContext();
    int inheritedChanges = 0;
    int ownChanges = 0;
    connect(inherited, &SingingClip::voiceContextChanged, [&] { ++inheritedChanges; });
    connect(independent, &SingingClip::voiceContextChanged, [&] { ++ownChanges; });

    SpeakerMixModel::SpeakerMixData mix;
    mix.mode = SpeakerMixModel::SingerSourceMode::FixedMix;
    mix.sources = {{SpeakerInfo("S2")}, {SpeakerInfo("S3")}};
    mix.fixedWeights = {0.7};
    track.setVoiceContext(singer, SpeakerInfo("S2"), mix);
    QCOMPARE(inheritedChanges, 1);
    QCOMPARE(ownChanges, 0);
    QCOMPARE(inherited->speakerId(), QStringLiteral("S2"));
    QVERIFY(inherited->effectiveVoiceContext().hasSameInferenceInput(track.voiceContext()));
    QVERIFY(independent->effectiveVoiceContext() == ownContext);

    independent->useTrackVoiceContext();
    QCOMPARE(ownChanges, 1);
    QVERIFY(independent->usesTrackVoiceContext());
    QVERIFY(independent->effectiveVoiceContext().hasSameInferenceInput(track.voiceContext()));
    track.setVoiceContext(singer, SpeakerInfo("S2"), mix);
    QCOMPARE(inheritedChanges, 1);
    QCOMPARE(ownChanges, 1);
}

void VoiceAndInferenceTests::voiceContextSpeakerMetadataCopiesAreIndependent() {
    SpeakerInfo original("S1", "Speaker");
    auto edited = original;
    QVERIFY(edited == original);
    edited.setToneRange(std::make_pair(48, 84));
    edited.setMixable(true);
    QVERIFY(edited != original);
    QVERIFY(!original.toneRange());
    QVERIFY(!original.mixable());
    QCOMPARE(edited.toneRange(), std::optional(std::make_pair(48, 84)));
    QVERIFY(edited.mixable());
}

void VoiceAndInferenceTests::voiceContextSingerCapabilityCopiesAreIndependent() {
    SingerInfo original({"singer", "package", QVersionNumber(1, 0)}, "Singer");
    QVERIFY(!original.capability());
    SingerCapabilitySummary capability;
    capability.mixableSpeakers = {"S1", "S2"};
    capability.acousticParameters = QStringList{"breathiness", "voicing"};
    original.setCapability(capability);
    auto edited = original;
    QVERIFY(edited == original);
    auto editedCapability = *edited.capability();
    editedCapability.mixableSpeakers = {"S1"};
    editedCapability.vocoderPitchControllable = true;
    edited.setCapability(editedCapability);
    QVERIFY(edited != original);
    QVERIFY(original.capability() == std::optional(capability));
    QVERIFY(edited.capability() == std::optional(editedCapability));
}
