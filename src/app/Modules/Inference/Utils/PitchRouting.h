#ifndef PITCHROUTING_H
#define PITCHROUTING_H

#include <QList>

namespace PitchRouting {
    inline constexpr char VocoderPitchTag[] = "vocoder_pitch";

    [[nodiscard]] QList<double> applyToneShift(QList<double> pitch,
                                               const QList<double> &toneShiftCents);
    [[nodiscard]] QList<float> midiPitchToF0(const QList<double> &pitch,
                                             double sourceIntervalSeconds, qsizetype targetFrames,
                                             double targetIntervalSeconds);
}

#endif // PITCHROUTING_H
