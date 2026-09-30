#include "ExtractMidiTask.h"

#include <algorithm>

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
            // The language only conditions the model, which produces notes for any language. A
            // language that the model does not declare is therefore replaced by the declared
            // default rather than passed through, because the model rejects an undeclared
            // language and the extraction would fail.
            const auto wanted = m_input.language.toStdString();
            const auto *schema = notes.spec().exports()->as<Note::NoteSchema>();
            if (!wanted.empty() && schema &&
                std::find(schema->languages.begin(), schema->languages.end(), wanted) !=
                    schema->languages.end()) {
                input.language = wanted;
            }
            input.progress = progress;
            return notes.start(input);
        },
        [this](const Note::NoteResult &transcription) {
            // Seconds are converted to ticks for each note through the timeline. If the tempo
            // changes, applying a single tempo to the whole take causes a drift that grows with
            // the take length and misaligns the notes with the audio.
            //
            // Ticks are relative to the audio clip because the notes are inserted into a singing
            // clip with the same start as the audio clip, and a note start is measured from the
            // start of its clip.
            result.reserve(result.size() + transcription.notes.size());
            for (const auto &note : transcription.notes) {
                const auto startMs = m_input.audioMaterialOriginMs + note.start * 1000.0;
                const auto endMs = startMs + note.duration * 1000.0;
                const auto startTick =
                    m_input.timeline.msToTick(startMs) - m_input.audioClipStartTick;
                const auto endTick = m_input.timeline.msToTick(endMs) - m_input.audioClipStartTick;
                const auto start = static_cast<int>(qRound(startTick));
                const auto length = static_cast<int>(qRound(endTick - startTick));
                // A note shorter than half a tick rounds to zero length. The project model stores
                // notes in an interval tree that rejects empty intervals, so such a note is
                // dropped here rather than inserted.
                if (length <= 0) {
                    continue;
                }
                result.push_back({note.key, start, length});
            }
        });
    if (!success()) {
        result.clear();
    }
}
