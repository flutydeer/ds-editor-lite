#include "Model/AppModel/EffectiveVoiceContext.h"
#include "Model/AppModel/Track.h"

#include <QCoreApplication>

#include <cstdio>

namespace {
    bool expect(const bool condition, const char *message) {
        if (condition)
            return true;
        std::fprintf(stderr, "FAILED: %s\n", message);
        return false;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    bool ok = true;

    Track track;
    int notificationCount = 0;
    VoiceContextChange received;
    QObject::connect(&track, &Track::voiceContextChanged,
                     [&notificationCount, &received](const VoiceContextChange &change) {
                         ++notificationCount;
                         received = change;
                     });

    SingerInfo singer({"singer", "package", QVersionNumber(1, 0)}, "Test Singer");
    SpeakerInfo speaker("speaker");
    SpeakerMixModel::SpeakerMixData mix;
    mix.mode = SpeakerMixModel::SingerSourceMode::FixedMix;
    mix.sources = {{SpeakerInfo("speaker")}, {SpeakerInfo("speaker-b")}};
    mix.fixedWeights = {0.4};

    const auto before = track.voiceContext();
    track.setVoiceContext(singer, speaker, mix);
    const auto after = track.voiceContext();

    ok &= expect(notificationCount == 1, "one voice context notification");
    ok &= expect(received.before == before, "notification contains the previous context");
    ok &= expect(received.after == after, "notification contains the final context");

    track.setVoiceContext(singer, speaker, mix);
    ok &= expect(notificationCount == 1, "equal context does not notify again");

    auto changedMix = mix;
    changedMix.fixedWeights = {0.7};
    track.setSpeakerMixData(changedMix);
    ok &= expect(notificationCount == 2, "speaker mix change emits one notification");
    ok &= expect(received.before == after, "speaker mix event contains its previous context");
    ok &= expect(received.after == track.voiceContext(),
                 "speaker mix event contains its final context");

    auto inherited = after;
    inherited.followsTrack = true;
    ok &= expect(after.hasSameInferenceInput(inherited),
                 "inheritance-only changes do not change inference input");

    return ok ? 0 : 1;
}
