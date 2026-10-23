#ifndef EXTRACTMIDITASK_H
#define EXTRACTMIDITASK_H

#include <vector>
#include <optional>
#include <utility>

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

    /// Converts file-relative seconds to clip-local start and duration ticks. A zero duration is
    /// valid and can be dropped; malformed times or an unrepresentable note return no value.
    [[nodiscard]] static std::optional<std::pair<int, int>>
        noteTicks(const Input &input, double startSeconds, double durationSeconds);

    std::vector<ExtractMidiNote> result;

private:
    void runTask() override;
};
#endif // EXTRACTMIDITASK_H
