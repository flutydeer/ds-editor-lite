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
    /// \a startMs is where the span's first frame sits in the audio file, and \a intervalMs is the
    /// analyzer's own frame spacing -- read from what it produced rather than assumed, because
    /// the two analyzers shipped disagree about it and a wrong assumption silently stretches the
    /// curve.
    ResultSegment placeOnTimeline(const QList<double> &values, double startMs,
                                  double intervalMs) const;
};
#endif // EXTRACTPITCHTASK_H
