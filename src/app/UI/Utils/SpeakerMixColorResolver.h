#ifndef SPEAKERMIXCOLORRESOLVER_H
#define SPEAKERMIXCOLORRESOLVER_H

#include "Modules/PackageManager/Models/SpeakerInfo.h"

#include <QColor>
#include <QList>
#include <QString>

struct SpeakerMixColorSet {
    QColor accent;
    QColor areaFill;
    QColor dotFill;
};

namespace SpeakerMixColorResolver {
    int colorIndexForSpeaker(const QString &speakerId, const QList<SpeakerInfo> &referenceSpeakers,
                             int fallbackIndex);
    SpeakerMixColorSet colorsForSpeaker(const QString &speakerId,
                                        const QList<SpeakerInfo> &referenceSpeakers,
                                        int fallbackIndex);
}

#endif // SPEAKERMIXCOLORRESOLVER_H
