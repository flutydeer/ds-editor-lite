#include "ExtractMidiTask.h"

#include <otter/Api/Note/1/NoteApiL1.h>

namespace Note = otter::Api::Note::L1;

ExtractMidiTask::ExtractMidiTask(Input input) : ExtractTask(std::move(input)) {
    TaskStatus status;
    status.title = tr("Extract MIDI");
    status.message =
        tr("Pending infer: %1")
            .arg(m_input.displayAudioPath.isEmpty() ? m_input.audioPath : m_input.displayAudioPath);
    setStatus(status);
}

void ExtractMidiTask::runTask() {
    runAnalysis<Note::NoteExecutive, Note::NoteSchema>(
        tr("Note"),
        [this](Note::NoteExecutive &notes, const Span &span, const auto &progress) {
            Note::NoteStartInput input;
            input.audio.sampleRate = span.sampleRate;
            input.audio.channelCount = 1;
            input.audio.samples.assign(span.samples.begin() + span.begin,
                                       span.samples.begin() + span.end);
            input.audio.startTime = span.startTime;
            if (!m_input.language.isEmpty()) {
                input.language = m_input.language.toStdString();
            }
            input.progress = progress;
            return notes.start(input);
        },
        [this](const Note::NoteResult &transcription) {
            // Seconds to ticks, note by note through the timeline. Applying one tempo to the
            // whole take drifts further the longer the take runs on a piece whose tempo changes,
            // and the notes then sit next to the audio rather than on it.
            //
            // Ticks are local to the audio clip: the notes are inserted into a singing clip that
            // starts where the audio clip does, and a note's start is measured from the start of
            // its clip.
            result.reserve(result.size() + transcription.notes.size());
            for (const auto &note : transcription.notes) {
                const auto startMs = m_input.audioMaterialOriginMs + note.start * 1000.0;
                const auto endMs = startMs + note.duration * 1000.0;
                const auto startTick =
                    m_input.timeline.msToTick(startMs) - m_input.audioClipStartTick;
                const auto endTick = m_input.timeline.msToTick(endMs) - m_input.audioClipStartTick;
                result.push_back({note.key, static_cast<int>(qRound(startTick)),
                                  static_cast<int>(qRound(endTick - startTick))});
            }
        });
    if (!success()) {
        result.clear();
    }
}
