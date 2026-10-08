#ifndef EXTRACTPITCHTASK_H
#define EXTRACTPITCHTASK_H

#include <QList>

#include "ExtractTask.h"

class ExtractPitchTask final : public ExtractTask {
    Q_OBJECT

public:
    struct ResultSegment {
        int globalStartTick = 0;
        QList<double> values;
    };

    explicit ExtractPitchTask(Input input);

    QList<ResultSegment> result;

private:
    void runTask() override;
    static double freqToMidi(double frequency);

    /// Places one span's curve on the project timeline.
    ///
    /// \a startMs is the position of the first frame of the span in the audio file. \a intervalMs
    /// is the frame interval of the analyzer, read from the analyzer result rather than assumed,
    /// because the two shipped analyzers use different intervals and a wrong value silently
    /// stretches the curve.
    ResultSegment placeOnTimeline(const QList<double> &values, double startMs,
                                  double intervalMs) const;
};
#endif // EXTRACTPITCHTASK_H
