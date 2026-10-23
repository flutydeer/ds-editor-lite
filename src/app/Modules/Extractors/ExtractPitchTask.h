#ifndef EXTRACTPITCHTASK_H
#define EXTRACTPITCHTASK_H

#include <QList>
#include <optional>
#include <otter/Api/F0/1/F0ApiL1.h>

#include "ExtractTask.h"

class ExtractPitchTask final : public ExtractTask {
    Q_OBJECT

public:
    struct ResultSegment {
        int globalStartTick = 0;
        QList<double> values;
    };

    explicit ExtractPitchTask(Input input);

    /// Converts aligned, finite frames with valid timing; an absent mask is inferred from F0.
    [[nodiscard]] static std::optional<QList<double>>
        pitchValues(const otter::Api::F0::L1::F0Result &curve);

    QList<ResultSegment> result;

private:
    void runTask() override;
    static double freqToMidi(double frequency);

    /// Place the curve using the analyzer's file-relative start and frame interval in milliseconds.
    ResultSegment placeOnTimeline(const QList<double> &values, double startMs,
                                  double intervalMs) const;
};
#endif // EXTRACTPITCHTASK_H
