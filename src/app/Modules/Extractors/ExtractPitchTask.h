#ifndef EXTRACTPITCHTASK_H
#define EXTRACTPITCHTASK_H

#include "ExtractTask.h"

#include <synthrt/Extract/PitchExtractor.h>

#include <QMutex>

class ExtractPitchTask final : public ExtractTask {
    Q_OBJECT

public:
    struct ResultSegment {
        int globalStartTick = 0;
        QList<double> values;
    };

    explicit ExtractPitchTask(Input input);

    void terminate() override;

    QList<ResultSegment> result;

private:
    void runTask() override;
    static std::vector<float> freqToMidi(const std::vector<float> &frequencies);
    ResultSegment processOutput(const QList<double> &values, double frameOffsetMs) const;

    mutable QMutex m_extractorMutex;
    srt::core::NO<srt::extract::PitchExtractor> m_extractor;
};
#endif // EXTRACTPITCHTASK_H
