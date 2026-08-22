#ifndef PIANOROLLNOTECOMMIT_H
#define PIANOROLLNOTECOMMIT_H

#include "Automation/AutomationTypes.h"
#include "Automation/ProjectAutomationDtos.h"

#include <optional>

namespace Automation {
    class CoreRuntime;
}

namespace PianoRollNoteCommit {
    [[nodiscard]] std::optional<Automation::NoteId> insert(Automation::CoreRuntime &runtime,
                                                           Automation::ClipId clipId,
                                                           Automation::NoteDraftDto draft);
    [[nodiscard]] std::optional<Automation::NoteId>
        split(Automation::CoreRuntime &runtime, Automation::ClipId clipId,
              Automation::NoteId originalNoteId, Automation::NoteDraftDto draft, int newLength);
}

#endif // PIANOROLLNOTECOMMIT_H
