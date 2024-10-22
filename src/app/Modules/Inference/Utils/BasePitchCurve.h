#ifndef BASEPITCHCURVE_H
#define BASEPITCHCURVE_H

#include <vector>
#include <dsonnxinfer/DsProject.h>

// https://github.com/yqzhishen/opensvip/blob/main/csharp/Plugins/Ace/BasePitchCurve.cs

class BasePitchCurve {
public:
    explicit BasePitchCurve(const dsonnxinfer::Segment &segment);
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
