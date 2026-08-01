#ifndef CLIPSINFO_H
#define CLIPSINFO_H

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
};

#endif // CLIPSINFO_H
