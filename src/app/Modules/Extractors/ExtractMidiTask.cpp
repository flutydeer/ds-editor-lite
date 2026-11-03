#include "ExtractMidiTask.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <otter/Api/Note/1/NoteApiL1.h>

namespace Note = otter::Api::Note::L1;

ExtractMidiTask::ExtractMidiTask(Input input) : ExtractTask(std::move(input)) {
    TaskStatus status;
    status.title = tr("Extract MIDI");
    status.message = tr("Pending infer: %1").arg(displayPath());
    setStatus(status);
}

std::optional<std::pair<int, int>> ExtractMidiTask::noteTicks(
    const Input &input, const double startSeconds, const double durationSeconds) {
    if (!std::isfinite(startSeconds) || !std::isfinite(durationSeconds) || durationSeconds < 0 ||
        !std::isfinite(input.audioMaterialOriginMs))
        return std::nullopt;

    const auto startMs = input.audioMaterialOriginMs + startSeconds * 1000.0;
    const auto endMs = startMs + durationSeconds * 1000.0;
    if (!std::isfinite(startMs) || !std::isfinite(endMs))
        return std::nullopt;
    const auto startTick = input.timeline.msToTick(startMs) - input.audioClipStartTick;
    const auto endTick = input.timeline.msToTick(endMs) - input.audioClipStartTick;
    const auto durationTicks = endTick - startTick;
    if (!std::isfinite(startTick) || !std::isfinite(endTick) ||
        !std::isfinite(durationTicks) || durationTicks < 0)
        return std::nullopt;

    // Round before range checking, without converting to int. start + duration must also fit int.
    const auto start = std::round(startTick);
    const auto duration = std::round(durationTicks);
    constexpr auto minimum = std::numeric_limits<int>::min();
    constexpr auto maximum = std::numeric_limits<int>::max();
    if (start < minimum || start > maximum || duration > maximum || start + duration > maximum)
        return std::nullopt;
    return std::pair{static_cast<int>(start), static_cast<int>(duration)};
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
            // Use the analyzer's default for unsupported languages.
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
            // Convert each note through the tempo map, relative to the audio clip's start tick.
            result.reserve(result.size() + transcription.notes.size());
            for (const auto &note : transcription.notes) {
                const auto ticks = noteTicks(m_input, note.start, note.duration);
                if (!ticks) {
                    fail(ErrorCode::ModelRunFailed,
                         tr("The note analyzer returned invalid note timing"));
                    return;
                }
                const auto [start, length] = *ticks;
                // Drop sub-tick notes that round to zero length; the project interval tree rejects
                // them.
                if (length <= 0) {
                    continue;
                }
                result.push_back({note.key, start, length, note.confidence});
            }
        });
    if (!success()) {
        result.clear();
    }
}
