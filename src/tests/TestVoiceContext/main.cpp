#include <lite/ProjectModel/AppModel/EffectiveVoiceContext.h>
#include <lite/ProjectModel/AppModel/Track.h>

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
};

QTEST_GUILESS_MAIN(VoiceContextTests)
#include "main.moc"
