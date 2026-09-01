#ifndef SPEAKERMIXDATA_H
#define SPEAKERMIXDATA_H

#include <lite/ProjectModel/Voice/SpeakerInfo.h>
#include <lite/Core/IdGenerator.h>

#include <QList>
#include <QString>
#include <QVector>

namespace SpeakerMixModel {

    enum class SingerSourceMode { Single, FixedMix, DynamicMix };

    struct SpeakerMixSource {
        SpeakerInfo speaker;

        bool operator==(const SpeakerMixSource &other) const;
        bool operator!=(const SpeakerMixSource &other) const;
    };

    struct SpeakerMixKeyframe {
        int tick = 0;
        QVector<double> weights;
        int id = IdGenerator::instance()->next();

        bool operator==(const SpeakerMixKeyframe &other) const;
        bool operator!=(const SpeakerMixKeyframe &other) const;
    };

    struct SpeakerMixData {
        SingerSourceMode mode = SingerSourceMode::Single;
        bool dynamicBypassed = false;
        QList<SpeakerMixSource> sources;
        QVector<double> fixedWeights;
        QList<SpeakerMixKeyframe> dynamicKeyframes;
        QString sourcePresetId;
        QString sourcePresetName;
        bool sourcePresetDirty = false;

        bool operator==(const SpeakerMixData &other) const;
        bool operator!=(const SpeakerMixData &other) const;
    };

    SpeakerMixData normalizeSpeakerMixData(const SpeakerMixData &data);
    SpeakerMixData preservePresetSourceAsDirty(const SpeakerMixData &oldData,
                                               SpeakerMixData newData);
    bool isSpeakerMixDataSingle(const SpeakerMixData &data);
    bool hasDynamicMixAutomation(const SpeakerMixData &data);
    bool isDynamicMixActive(const SpeakerMixData &data);
    bool isDynamicMixBypassed(const SpeakerMixData &data);
    QVector<double> normalizeSpeakerMixFullWeights(const QVector<double> &weights, int sourceCount);
    QVector<double> explicitWeightsFromFullWeights(const QVector<double> &weights);
    QVector<double> fullWeightsFromExplicitWeights(const QVector<double> &weights);

} // namespace SpeakerMixModel

#endif // SPEAKERMIXDATA_H
