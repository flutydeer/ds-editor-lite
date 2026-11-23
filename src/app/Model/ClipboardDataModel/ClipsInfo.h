#ifndef CLIPSINFO_H
#define CLIPSINFO_H

#include "Automation/ProjectAutomationDtos.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>

class Clip;

class ClipsInfo {
public:
    QList<Clip *> clips;
    QList<int> trackIndexOffsets;
    // Route language each copied clip was built for, and whether the copied clip
    // recorded one, parallel to `clips`. The clipboard keeps no track context,
    // so the paste path restores it to let an inherited clip cascade into the
    // target track's language, including a route that resolved to no language.
    QList<QString> sourceFallbackLanguages;
    QList<bool> sourceFallbackKnown;

    static QJsonObject serializeToJson(const ClipsInfo &info);
    static ClipsInfo deserializeFromJson(const QJsonObject &obj);

    [[nodiscard]] QList<Automation::ClipInsertDto> preparePaste(const QList<Track *> &tracks,
                                                                int tick, int trackIndex) const;
};

#endif // CLIPSINFO_H
