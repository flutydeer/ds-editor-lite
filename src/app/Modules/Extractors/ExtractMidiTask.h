#ifndef EXTRACTMIDITASK_H
#define EXTRACTMIDITASK_H

#include <vector>

#include "ExtractTask.h"

struct ExtractMidiNote {
    int note = 0;
    int start = 0;
    int duration = 0;
    /// Confidence the model reported for the note, in the inclusive range from zero to one. It is
    /// diagnostic: no surface of the editor displays it yet.
    double confidence = 0;
};

class ExtractMidiTask final : public ExtractTask {
    Q_OBJECT

public:
    explicit ExtractMidiTask(Input input);

    std::vector<ExtractMidiNote> result;

private:
    void runTask() override;
};
#endif // EXTRACTMIDITASK_H
