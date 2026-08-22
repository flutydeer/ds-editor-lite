#include "Controller/PianoRollNoteCommit.h"
#include "TestRuntime.h"

#include <QCoreApplication>
#include <QTextStream>

#include <optional>

namespace {
    int failures = 0;
    int assertions = 0;

    void expect(const bool condition, const char *message) {
        ++assertions;
        if (condition)
            return;
        ++failures;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
    }

    Automation::CommandContext commandContext(const Automation::CoreRuntime &runtime) {
        return {
            .expected = runtime.documentVersion(),
            .source = Automation::InvocationSource::Test,
        };
    }

    struct Fixture {
        Automation::TrackId trackId;
        Automation::ClipId clipId;
    };

    std::optional<Fixture> createFixture(Automation::CoreRuntime &runtime) {
        Automation::TrackDraftDto track;
        track.clientRef = QStringLiteral("note-commit-track");
        track.name = QStringLiteral("Note Commit Track");
        track.gain = 1.0;
        track.defaultLanguage = QStringLiteral("en");
        const auto insertedTrack = runtime.project().insertTrack(commandContext(runtime), 0, track);
        if (!insertedTrack || insertedTrack.get().affectedObjects.size() != 1)
            return std::nullopt;

        Automation::ClipDraftDto clip;
        clip.clientRef = QStringLiteral("note-commit-clip");
        clip.type = Automation::ClipDraftDto::Type::Singing;
        clip.properties.name = QStringLiteral("Note Commit Clip");
        clip.properties.length = 1920;
        clip.properties.clipLen = 1920;
        clip.properties.gain = 1.0;
        clip.defaultLanguage = QStringLiteral("en");
        const Automation::TrackId trackId(insertedTrack.get().affectedObjects.first().value);
        const auto insertedClip = runtime.project().insertClips(
            commandContext(runtime), {
                                         {.trackId = trackId, .clip = clip}
        });
        if (!insertedClip || insertedClip.get().affectedObjects.size() != 1)
            return std::nullopt;
        return Fixture{
            .trackId = trackId,
            .clipId = Automation::ClipId(insertedClip.get().affectedObjects.first().value),
        };
    }

    Automation::NoteDraftDto noteDraft(const int start, const int length, const QString &lyric) {
        return {
            .clientRef = QStringLiteral("caller-owned-ref"),
            .localStart = start,
            .length = length,
            .keyIndex = 64,
            .lyric = lyric,
            .language = QStringLiteral("en"),
        };
    }

    std::optional<Automation::NoteSnapshotDto> findNote(Automation::CoreRuntime &runtime,
                                                        const Automation::ClipId clipId,
                                                        const Automation::NoteId noteId) {
        const auto notes = runtime.notes().getNotes(runtime.documentVersion().documentId, clipId);
        if (!notes)
            return std::nullopt;
        for (const auto &note : notes.get()) {
            if (note.id == noteId)
                return note;
        }
        return std::nullopt;
    }

    qsizetype noteCount(Automation::CoreRuntime &runtime, const Automation::ClipId clipId) {
        const auto notes = runtime.notes().getNotes(runtime.documentVersion().documentId, clipId);
        return notes ? notes.get().size() : -1;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    AutomationTestSupport::TestRuntime testRuntime;
    auto &runtime = testRuntime.runtime();
    const auto fixture = createFixture(runtime);
    expect(fixture.has_value(), "fixture must create a singing clip through the Facade");
    if (!fixture)
        return 1;

    const auto beforeInsert = runtime.documentVersion();
    const auto inserted = PianoRollNoteCommit::insert(runtime, fixture->clipId,
                                                      noteDraft(0, 480, QStringLiteral("la")));
    const auto insertedSnapshot =
        inserted ? findNote(runtime, fixture->clipId, *inserted) : std::nullopt;
    expect(inserted.has_value() && inserted->isValid(),
           "GUI insert adapter must return the Facade-created NoteId");
    expect(insertedSnapshot && insertedSnapshot->data.localStart == 0 &&
               insertedSnapshot->data.length == 480 && insertedSnapshot->data.clientRef.isEmpty(),
           "returned insert id must resolve to the committed note without persisting client_ref");
    expect(runtime.documentVersion().revision == beforeInsert.revision + 1,
           "successful GUI insert must advance exactly one revision");

    const auto beforeInvalidInsert = runtime.documentVersion();
    const auto countBeforeInvalidInsert = noteCount(runtime, fixture->clipId);
    const auto invalidInsert = PianoRollNoteCommit::insert(
        runtime, fixture->clipId, noteDraft(480, 0, QStringLiteral("invalid")));
    expect(!invalidInsert && runtime.documentVersion() == beforeInvalidInsert &&
               noteCount(runtime, fixture->clipId) == countBeforeInvalidInsert,
           "rejected GUI insert must return no id and leave document state unchanged");

    const auto missingClipInsert = PianoRollNoteCommit::insert(
        runtime, Automation::ClipId(999999), noteDraft(480, 240, QStringLiteral("missing")));
    expect(!missingClipInsert && runtime.documentVersion() == beforeInvalidInsert,
           "missing target clip must return no id without advancing revision");

    const auto beforeSplit = runtime.documentVersion();
    const auto childDraft = noteDraft(240, 240, QStringLiteral("-"));
    const auto child =
        PianoRollNoteCommit::split(runtime, fixture->clipId, *inserted, childDraft, 240);
    const auto originalAfterSplit = findNote(runtime, fixture->clipId, *inserted);
    const auto childAfterSplit = child ? findNote(runtime, fixture->clipId, *child) : std::nullopt;
    expect(child && child->isValid() && *child != *inserted,
           "GUI split adapter must return a distinct Facade-created child NoteId");
    expect(originalAfterSplit && originalAfterSplit->data.length == 240 && childAfterSplit &&
               childAfterSplit->data.localStart == 240 && childAfterSplit->data.length == 240 &&
               childAfterSplit->data.clientRef.isEmpty(),
           "split result ids must resolve to the shortened original and committed child");
    expect(runtime.documentVersion().revision == beforeSplit.revision + 1 &&
               noteCount(runtime, fixture->clipId) == 2,
           "successful GUI split must commit once and add exactly one note");

    const auto beforeInvalidSplit = runtime.documentVersion();
    const auto countBeforeInvalidSplit = noteCount(runtime, fixture->clipId);
    const auto invalidSplit = PianoRollNoteCommit::split(
        runtime, fixture->clipId, *inserted, noteDraft(120, 120, QStringLiteral("-")), 0);
    const auto missingNoteSplit =
        PianoRollNoteCommit::split(runtime, fixture->clipId, Automation::NoteId(999999),
                                   noteDraft(480, 120, QStringLiteral("-")), 120);
    expect(!invalidSplit && !missingNoteSplit && runtime.documentVersion() == beforeInvalidSplit &&
               noteCount(runtime, fixture->clipId) == countBeforeInvalidSplit,
           "rejected GUI splits must return no id and preserve model and revision");

    QTextStream(stdout) << "Piano-roll note commit: " << assertions << " assertions, " << failures
                        << " failures" << Qt::endl;
    return failures == 0 ? 0 : 1;
}
