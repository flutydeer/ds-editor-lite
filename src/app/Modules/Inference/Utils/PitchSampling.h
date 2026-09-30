#ifndef PITCHSAMPLING_H
#define PITCHSAMPLING_H

#include <QList>

namespace PitchSampling {

    /// Samples a pitch curve, in which a value of zero or less marks an unvoiced frame, at the
    /// target positions.
    ///
    /// Every step that moves a pitch curve between grids uses this function: placing an extracted
    /// curve on the 5-tick grid, sampling the input curve at the inference frames, and aligning the
    /// original pitch with the vocoder frames. PitchRouting treats a value of zero or less as
    /// unvoiced, so each of these steps must keep the marker intact.
    ///
    /// A target between two voiced frames receives the linear interpolation of both frames. A
    /// target adjacent to an unvoiced frame receives the value of the nearer frame, and the left
    /// frame if both are equally near. Interpolating across a voicing boundary would produce a
    /// value between a pitch and zero, which is neither a pitch nor the unvoiced marker. A target
    /// outside the source range receives the value of the nearest endpoint.
    ///
    /// \a sourcePositions must be strictly increasing and of the same size as \a values.
    /// \return the sampled values, one per target position, or an empty list if \a values is
    /// empty or the positions are invalid.
    QList<double> resample(const QList<double> &values, const QList<double> &sourcePositions,
                           const QList<double> &targetPositions);

}

#endif // PITCHSAMPLING_H
