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

    static QJsonObject serializeToJson(const ClipsInfo &info);
    static ClipsInfo deserializeFromJson(const QJsonObject &obj);

    [[nodiscard]] QList<Automation::ClipInsertDto> preparePaste(const QList<Track *> &tracks,
                                                                int tick, int trackIndex) const;
};

#endif // CLIPSINFO_H
