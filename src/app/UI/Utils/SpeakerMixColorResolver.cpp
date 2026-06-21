#include "SpeakerMixColorResolver.h"

#include "AppColorPalette.h"

namespace SpeakerMixColorResolver {
    int colorIndexForSpeaker(const QString &speakerId,
                             const QList<SpeakerInfo> &referenceSpeakers,
                             const int fallbackIndex) {
        for (int i = 0; i < referenceSpeakers.size(); ++i) {
            if (referenceSpeakers[i].id() == speakerId)
                return i;
        }
        return fallbackIndex;
    }

    SpeakerMixColorSet colorsForSpeaker(const QString &speakerId,
                                        const QList<SpeakerInfo> &referenceSpeakers,
                                        const int fallbackIndex) {
        const int index = colorIndexForSpeaker(speakerId, referenceSpeakers, fallbackIndex);
        auto *palette = AppColorPalette::instance();
        return {palette->baseColor(index), palette->speakerMixParamFill(index),
                palette->speakerMixDotFill(index)};
    }
}
