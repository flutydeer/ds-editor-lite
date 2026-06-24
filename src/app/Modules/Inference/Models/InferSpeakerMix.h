#ifndef INFERSPEAKERMIX_H
#define INFERSPEAKERMIX_H

#include "Model/AppModel/SpeakerMixData.h"
#include "Model/AppModel/Timeline.h"

#include <QList>
#include <QString>
#include <QVector>

struct InferSpeakerMixSource {
    QString speaker;
    double interval = 0;
    QVector<double> proportions;

    [[nodiscard]] bool isValid() const;
    bool operator==(const InferSpeakerMixSource &other) const;
    bool operator!=(const InferSpeakerMixSource &other) const;
};

struct InferSpeakerMix {
    QString fallbackSpeaker;
    QList<InferSpeakerMixSource> sources;

    [[nodiscard]] bool isEmpty() const;
    [[nodiscard]] QString signature() const;
    bool operator==(const InferSpeakerMix &other) const;
    bool operator!=(const InferSpeakerMix &other) const;
};

namespace InferSpeakerMixModel {
    InferSpeakerMix staticSpeakerMix(const QString &speaker);
    InferSpeakerMix fixedSpeakerMixFromData(const SpeakerMixModel::SpeakerMixData &data,
                                            const QString &fallbackSpeaker);
    InferSpeakerMix
        effectiveSpeakerMixForFixedInference(const SpeakerMixModel::SpeakerMixData &data,
                                             const QString &fallbackSpeaker);
    InferSpeakerMix dynamicSpeakerMixFromData(const SpeakerMixModel::SpeakerMixData &data,
                                              const QString &fallbackSpeaker, int startTick,
                                              int endTick, const Timeline &timeline,
                                              double intervalSeconds = 0.01);
    InferSpeakerMix effectiveSpeakerMixFromData(const SpeakerMixModel::SpeakerMixData &data,
                                                const QString &fallbackSpeaker, int startTick,
                                                int endTick, const Timeline &timeline,
                                                double intervalSeconds = 0.01);
}

#endif // INFERSPEAKERMIX_H
