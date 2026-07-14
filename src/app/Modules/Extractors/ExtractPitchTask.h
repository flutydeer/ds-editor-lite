//
// Created by fluty on 24-11-13.
//

#ifndef EXTRACTPITCHTASK_H
#define EXTRACTPITCHTASK_H

#include "ExtractTask.h"

#include <synthrt/Extract/PitchExtractor.h>

#include <QMutex>

class ExtractPitchTask final : public ExtractTask {
    Q_OBJECT

public:
    explicit ExtractPitchTask(Input input);

    void terminate() override;

    QList<QPair<double, QList<double>>> result;

private:
    void runTask() override;
    static std::vector<float> freqToMidi(const std::vector<float> &frequencies);
    QList<double> processOutput(const QList<double> &values) const;

    mutable QMutex m_extractorMutex;
    srt::core::NO<srt::extract::PitchExtractor> m_extractor;
};
#endif // EXTRACTPITCHTASK_H
