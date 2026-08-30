#include "PianoRollNoteCommit.h"

#include "Automation/CoreRuntime.h"

#include <QDebug>

namespace {
    const QString insertedNoteClientRef = QStringLiteral("gui-inserted-note");
    const QString splitNoteClientRef = QStringLiteral("gui-split-note");

    Automation::CommandContext commandContext(const Automation::CoreRuntime &runtime) {
        return {
            .expected = runtime.documentVersion(),
            .source = Automation::InvocationSource::TrustedGui,
        };
    }

    std::optional<Automation::NoteId>
        createdNoteId(const Automation::AutomationResult<Automation::MutationResult> &result,
                      const QString &clientRef) {
        if (!result || !result.get().changed)
            return std::nullopt;
        for (const auto &created : result.get().createdObjects) {
            if (created.clientRef == clientRef &&
                created.object.kind == Automation::ObjectKind::Note) {
                const Automation::NoteId id(created.object.value);
                if (id.isValid())
                    return id;
            }
        }
        qCritical() << "Automation note commit did not return its created object binding";
        return std::nullopt;
    }
}

namespace PianoRollNoteCommit {
    std::optional<Automation::NoteId> insert(Automation::CoreRuntime &runtime,
                                             const Automation::ClipId clipId,
                                             Automation::NoteDraftDto draft) {
        draft.clientRef = insertedNoteClientRef;
        return createdNoteId(
            runtime.notes().insertNotes(commandContext(runtime), clipId, {std::move(draft)}),
            insertedNoteClientRef);
    }

    std::optional<Automation::NoteId> split(Automation::CoreRuntime &runtime,
                                            const Automation::ClipId clipId,
                                            const Automation::NoteId originalNoteId,
                                            Automation::NoteDraftDto draft, const int newLength) {
        draft.clientRef = splitNoteClientRef;
        return createdNoteId(runtime.notes().splitNote(commandContext(runtime), clipId,
                                                       originalNoteId, std::move(draft), newLength),
                             splitNoteClientRef);
    }
}
