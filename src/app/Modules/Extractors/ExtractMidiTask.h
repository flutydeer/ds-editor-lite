#ifndef EXTRACTMIDITASK_H
#define EXTRACTMIDITASK_H

#include "ExtractTask.h"

#include <vector>

#include <QMutex>

#include <otter/Analysis/AnalysisExecutive.h>

/// One transcribed note, in the editor's own units.
///
/// Ticks rather than seconds, because that is what the project is written in. The analyzer works
/// in seconds and knows nothing about tempo; converting here is what lets a piece whose tempo
/// changes come out in the right place.
struct ExtractMidiNote {
    int note = 0;
    int start = 0;
    int duration = 0;
};

class ExtractMidiTask final : public ExtractTask {
    Q_OBJECT

public:
    explicit ExtractMidiTask(Input input);

    void terminate() override;

    std::vector<ExtractMidiNote> result;

private:
    void runTask() override;

    mutable QMutex m_analyzerMutex;
    otter::AnalysisExecutive *m_analyzer = nullptr;
};
#endif // EXTRACTMIDITASK_H
