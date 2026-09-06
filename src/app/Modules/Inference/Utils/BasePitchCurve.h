#ifndef BASEPITCHCURVE_H
#define BASEPITCHCURVE_H

#include <QList>

#include <vector>

// https://github.com/yqzhishen/opensvip/blob/main/csharp/Plugins/Ace/BasePitchCurve.cs

namespace srt::svs::Api::Common::L1 {
    struct InputWordInfo;
}
class InferWord;

class BasePitchCurve {
public:
    struct InputNote {
        int key = 0;
        int cents = 0;
        double duration = 0.0;
        bool isRest = false;

        bool operator==(const InputNote &other) const {
            return key == other.key && duration == other.duration && isRest == other.isRest;
        }
    };

    explicit BasePitchCurve(const std::vector<srt::svs::Api::Common::L1::InputWordInfo> &word);
    explicit BasePitchCurve(const QList<InferWord> &words);
    explicit BasePitchCurve(std::vector<InputNote> notes);
    std::vector<double> GetPitchPoints(double timestep) const;
    double SemitoneValueAt(double seconds) const;
    [[nodiscard]] bool isEmpty() const;

    static std::vector<InputNote> inputNotes(const QList<InferWord> &words);
    static std::vector<InputNote> fillRestKeys(std::vector<InputNote> notes);

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
