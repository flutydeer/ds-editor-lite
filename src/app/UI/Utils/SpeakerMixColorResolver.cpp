#include "SpeakerMixColorResolver.h"

#include "AppColorPalette.h"
#include <lite/GUI/Theme/ThemeManager.h>

namespace SpeakerMixColorResolver {
    int colorIndexForSpeaker(const QString &speakerId, const QList<SpeakerInfo> &referenceSpeakers,
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
        // Per-index plot colors live in the theme token files as speakerMix.plot.fill<i> /
        // speakerMix.plot.line<i>; the index aligns with the app color palette order.
        const int index = colorIndexForSpeaker(speakerId, referenceSpeakers, fallbackIndex);
        SpeakerMixColorSet colors;
        colors.accent = AppColorPalette::instance()->baseColor(index);
        colors.areaFill = ThemeManager::instance()->semanticColor(
            QStringLiteral("speakerMix.plot.fill%1").arg(index));
        colors.line = ThemeManager::instance()->semanticColor(
            QStringLiteral("speakerMix.plot.line%1").arg(index));
        return colors;
    }
}
