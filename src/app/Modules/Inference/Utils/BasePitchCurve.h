#ifndef BASEPITCHCURVE_H
#define BASEPITCHCURVE_H

#include <vector>

// https://github.com/yqzhishen/opensvip/blob/main/csharp/Plugins/Ace/BasePitchCurve.cs

namespace ds::Api::Common::L1 {
    struct InputWordInfo;
}

class BasePitchCurve {
public:
    explicit BasePitchCurve(const std::vector<ds::Api::Common::L1::InputWordInfo> &word);
    std::vector<double> GetPitchPoints(double timestep) const;
    double SemitoneValueAt(double seconds) const;

private:
    struct NoteInSeconds {
        double Start;
        double End;
        int Semitone;
    };

    std::vector<double> _valuesInSemitone;

    void Convolve(const std::vector<NoteInSeconds> &noteArray);
};

#endif // BASEPITCHCURVE_H
