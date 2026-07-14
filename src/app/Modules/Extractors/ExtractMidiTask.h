//
// Created by fluty on 24-11-13.
//

#ifndef EXTRACTMIDITASK_H
#define EXTRACTMIDITASK_H

#include "ExtractTask.h"

#include <synthrt/Extract/MidiExtractor.h>

#include <QMutex>

/// Local MIDI note struct, decoupled from synthrt types.
/// Field order (note, start, duration) preserves structured-binding compatibility
/// with MidiExtractController.
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

    mutable QMutex m_extractorMutex;
    srt::core::NO<srt::extract::MidiExtractor> m_extractor;
};
#endif // EXTRACTMIDITASK_H
