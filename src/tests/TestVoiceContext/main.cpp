#include <lite/ProjectModel/AppModel/EffectiveVoiceContext.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

#include <QtTest/QTest>

class VoiceContextTests final : public QObject {
    Q_OBJECT

private slots:

    void notificationsContainAtomicBeforeAndAfter() {
        Track track;
        int notificationCount = 0;
        VoiceContextChange received;
        QObject::connect(&track, &Track::voiceContextChanged,
                         [&](const VoiceContextChange &change) {
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

    void inheritanceOnlyChangePreservesInferenceInput() {
        Track track;
        const auto context = track.voiceContext();
        auto inherited = context;
        inherited.followsTrack = !context.followsTrack;
        QVERIFY(context.hasSameInferenceInput(inherited));
    }

    void inheritedClipsFollowTrackWhileOwnVoiceRemains() {
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

    void speakerMetadataCopiesAreIndependent() {
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

    void singerCapabilityCopiesAreIndependent() {
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
};

QTEST_GUILESS_MAIN(VoiceContextTests)
#include "main.moc"
