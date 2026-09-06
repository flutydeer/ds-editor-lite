#include "BasePitchCurve.h"

#include "Modules/Inference/Models/GenericInferModel.h"

#include <algorithm>
#include <cmath>

#include <diffsinger/Infer/dsinfer/Api/Inferences/Common/1/CommonApiL1.h>

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

void BasePitchCurve::Convolve(const std::vector<NoteInSeconds> &noteArray) {
    if (noteArray.empty() || noteArray.back().End <= 0.0) {
        _valuesInSemitone.clear();
        return;
    }
    const int totalPoints = static_cast<int>(std::round(1000 * (noteArray.back().End + 0.12))) + 1;
    std::vector initValues(totalPoints, 0.0);
    int noteIndex = 0;

    for (int i = 0; i < totalPoints; ++i) {
        initValues[i] = noteArray[noteIndex].Semitone;
        const double time = 0.001 * i;
        while (noteIndex < noteArray.size() - 1 &&
               time > 0.5 * (noteArray[noteIndex].End + noteArray[noteIndex + 1].Start))
            noteIndex++;
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

BasePitchCurve::BasePitchCurve(const std::vector<srt::svs::Api::Common::L1::InputWordInfo> &word) {
    std::vector<InputNote> notes;
    for (const auto &[phones, wordNotes] : word) {
        static_cast<void>(phones);
        for (const auto &[key, cents, duration, glide, is_rest] : wordNotes) {
            static_cast<void>(glide);
            notes.push_back({key, cents, duration, is_rest});
        }
    }
    *this = BasePitchCurve(std::move(notes));
}

BasePitchCurve::BasePitchCurve(const QList<InferWord> &words) {
    *this = BasePitchCurve(inputNotes(words));
}

std::vector<BasePitchCurve::InputNote> BasePitchCurve::inputNotes(const QList<InferWord> &words) {
    std::vector<InputNote> notes;
    for (const auto &word : words) {
        for (const auto &note : word.notes)
            notes.push_back({note.key, note.cents, note.duration, note.is_rest});
    }
    return notes;
}

BasePitchCurve::BasePitchCurve(std::vector<InputNote> notes) {
    notes = fillRestKeys(std::move(notes));
    std::vector<NoteInSeconds> noteArray;
    noteArray.reserve(notes.size());
    double startTime = 0.0;
    for (const auto &note : notes) {
        const auto duration = std::max(note.duration, 0.0);
        noteArray.push_back({startTime, startTime + duration, note.key});
        startTime += duration;
    }
    Convolve(noteArray);
}

std::vector<BasePitchCurve::InputNote> BasePitchCurve::fillRestKeys(std::vector<InputNote> notes) {
    const auto firstSinging = std::find_if(notes.begin(), notes.end(),
                                           [](const InputNote &note) { return !note.isRest; });
    if (firstSinging == notes.end())
        return notes;

    const auto firstIndex = static_cast<std::size_t>(std::distance(notes.begin(), firstSinging));
    for (std::size_t i = 0; i < firstIndex; ++i)
        notes[i].key = notes[firstIndex].key;

    std::size_t runStart = firstIndex + 1;
    auto leftIndex = firstIndex;
    while (runStart < notes.size()) {
        if (!notes[runStart].isRest) {
            leftIndex = runStart++;
            continue;
        }
        auto runEnd = runStart;
        while (runEnd < notes.size() && notes[runEnd].isRest)
            ++runEnd;
        if (runEnd == notes.size()) {
            for (auto i = runStart; i < runEnd; ++i)
                notes[i].key = notes[leftIndex].key;
            break;
        }

        const auto middle = runStart + (runEnd - runStart + 1) / 2;
        for (auto i = runStart; i < middle; ++i)
            notes[i].key = notes[leftIndex].key;
        for (auto i = middle; i < runEnd; ++i)
            notes[i].key = notes[runEnd].key;
        leftIndex = runEnd;
        runStart = runEnd + 1;
    }
    return notes;
}

std::vector<double> BasePitchCurve::GetPitchPoints(const double timestep) const {
    std::vector<double> pitchPoints;
    if (_valuesInSemitone.empty() || timestep <= 0.0)
        return pitchPoints;
    const double totalDuration = static_cast<double>(_valuesInSemitone.size()) / 1000.0;
    const int totalSteps = static_cast<int>(std::ceil(totalDuration / timestep));

    for (int step = 0; step < totalSteps; ++step) {
        const double time = step * timestep;
        pitchPoints.push_back(SemitoneValueAt(time));
    }

    return pitchPoints;
}

double BasePitchCurve::SemitoneValueAt(const double seconds) const {
    if (_valuesInSemitone.empty())
        return 0.0;
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

bool BasePitchCurve::isEmpty() const {
    return _valuesInSemitone.empty();
}
