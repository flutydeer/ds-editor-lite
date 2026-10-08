#include "PitchRouting.h"

#include "PitchSampling.h"

#include <algorithm>
#include <cmath>

QList<double> PitchRouting::applyToneShift(QList<double> pitch,
                                           const QList<double> &toneShiftCents) {
    if (pitch.size() != toneShiftCents.size())
        return {};

    for (qsizetype i = 0; i < pitch.size(); ++i) {
        if (pitch.at(i) > 0)
            pitch[i] += toneShiftCents.at(i) / 100.0;
    }
    return pitch;
}

QList<float> PitchRouting::midiPitchToF0(const QList<double> &pitch,
                                         const double sourceIntervalSeconds,
                                         const qsizetype targetFrames,
                                         const double targetIntervalSeconds) {
    if (pitch.isEmpty() || sourceIntervalSeconds <= 0 || targetFrames <= 0 ||
        targetIntervalSeconds <= 0) {
        return {};
    }

    QList<double> sourcePositions;
    sourcePositions.reserve(pitch.size());
    for (qsizetype i = 0; i < pitch.size(); ++i)
        sourcePositions.append(static_cast<double>(i) * sourceIntervalSeconds);

    QList<double> targetPositions;
    targetPositions.reserve(targetFrames);
    for (qsizetype i = 0; i < targetFrames; ++i)
        targetPositions.append(static_cast<double>(i) * targetIntervalSeconds);

    // A frame between a voiced and an unvoiced sample takes the nearer sample, so the result is
    // either a sung pitch or the unvoiced marker and never a value in between.
    const auto resampled = PitchSampling::resample(pitch, sourcePositions, targetPositions);
    if (resampled.size() != targetFrames)
        return {};

    QList<float> result;
    result.reserve(resampled.size());
    for (const auto midiPitch : resampled) {
        if (midiPitch <= 0) {
            result.append(0);
            continue;
        }
        result.append(static_cast<float>(440.0 * std::exp2((midiPitch - 69.0) / 12.0)));
    }
    return result;
}
