#ifndef DS_EDITOR_LITE_MIDIPARSETASK_H
#define DS_EDITOR_LITE_MIDIPARSETASK_H

#include <lite/ProjectConverters/MidiConverter.h>
#include <lite/Tasking/Task.h>

// Background MIDI parse: file -> reusable MidiParseData. No UI, no import
// options, no model mutation.
class MidiParseTask final : public Task {
public:
    explicit MidiParseTask(QString filePath, quint64 requestId);

    [[nodiscard]] quint64 requestId() const;
    MidiParseData takeResult();

private:
    void runTask() override;

    QString m_filePath;
    quint64 m_requestId = 0;
    MidiParseData m_result;
};

#endif // DS_EDITOR_LITE_MIDIPARSETASK_H
