#ifndef SPEAKERMIXPRESET_H
#define SPEAKERMIXPRESET_H

#include "Model/AppModel/SpeakerMixData.h"
#include "Modules/Inference/Models/SingerIdentifier.h"

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QVersionNumber>

struct SpeakerMixPreset {
    QString id;
    QString name;
    QString packageId;
    QString singerId;
    QVersionNumber packageVersion;
    QList<SpeakerMixModel::SpeakerMixSource> sources;
    QVector<double> fixedWeights;
    QDateTime createdAt;
    QDateTime updatedAt;

    [[nodiscard]] SingerIdentifier singerIdentifier() const;
    [[nodiscard]] QJsonObject toJson() const;
    static SpeakerMixPreset fromJson(const QJsonObject &obj);
};

#endif // SPEAKERMIXPRESET_H
