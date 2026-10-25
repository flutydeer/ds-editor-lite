#ifndef PITCHSAMPLING_H
#define PITCHSAMPLING_H

#include <QList>

namespace PitchSampling {

    /// Nonpositive values are unvoiced. Interpolate voiced frames linearly; at voicing boundaries,
    /// use the nearest frame (ties select the left). Outside the source range, use the nearest
    /// endpoint.
    /// sourcePositions must increase strictly and match values.size(); invalid positions return no
    /// samples.
    QList<double> resample(const QList<double> &values, const QList<double> &sourcePositions,
                           const QList<double> &targetPositions);

}

#endif // PITCHSAMPLING_H
