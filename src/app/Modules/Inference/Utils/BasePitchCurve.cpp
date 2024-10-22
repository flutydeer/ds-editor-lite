#include "BasePitchCurve.h"
#include <corecrt_math_defines.h>

void BasePitchCurve::Convolve(const std::vector<NoteInSeconds> &noteArray) {
    const int totalPoints = static_cast<int>(std::round(1000 * (noteArray.back().End + 0.12))) + 1;
    std::vector<double> initValues(totalPoints, 0.0);
    int noteIndex = 0;

    for (int i = 0; i < totalPoints; ++i) {
        initValues[i] = noteArray[noteIndex].Semitone;
        const double time = 0.001 * i;
        if (noteIndex < noteArray.size() - 1 &&
            time > 0.5 * (noteArray[noteIndex].End + noteArray[noteIndex + 1].Start)) {
            noteIndex++;
        }
    }

    std::vector<double> kernel(119);
    double sum = 0.0;
    for (int i = 0; i < 119; ++i) {
        const double time = 0.001 * (i - 59);
        kernel[i] = std::cos(M_PI * time / 0.12);
        sum += kernel[i];
    }
    for (auto &value : kernel) {
        value /= sum;
    }

    _valuesInSemitone.resize(totalPoints, 0.0);
    for (int i = 0; i < totalPoints; ++i) {
        for (int j = 0; j < 119; ++j) {
            const int clippedIndex = std::clamp(i - 59 + j, 0, totalPoints - 1);
            _valuesInSemitone[i] += initValues[clippedIndex] * kernel[j];
        }
    }
}

BasePitchCurve::BasePitchCurve(const dsonnxinfer::Segment &segment) {
    std::vector<NoteInSeconds> noteArray;
    double startTime = 0.0;

    for (const auto &[phones, notes] : segment.words) {
        for (const auto &[key, cents, duration, glide, is_rest] : notes) {
            noteArray.push_back({startTime, startTime + duration, key});
            startTime += duration;
        }
    }

    Convolve(noteArray);
}

std::vector<double> BasePitchCurve::GetPitchPoints(const double timestep) const {
    std::vector<double> pitchPoints;
    const double totalDuration = static_cast<double>(_valuesInSemitone.size()) / 1000.0;
    const int totalSteps = static_cast<int>(std::ceil(totalDuration / timestep));

    for (int step = 0; step < totalSteps; ++step) {
        const double time = step * timestep;
        pitchPoints.push_back(SemitoneValueAt(time));
    }

    return pitchPoints;
}

double BasePitchCurve::SemitoneValueAt(const double seconds) const {
    const double position = 1000 * std::max(seconds, 0.0);
    const double leftIndex = std::floor(position);
    const double lambda = position - leftIndex;

    const int clippedLeftIndex =
        std::min(static_cast<int>(leftIndex), static_cast<int>(_valuesInSemitone.size() - 1));
    const int clippedRightIndex =
        std::min(clippedLeftIndex + 1, static_cast<int>(_valuesInSemitone.size() - 1));

    return (1 - lambda) * _valuesInSemitone[clippedLeftIndex] +
           lambda * _valuesInSemitone[clippedRightIndex];
}
