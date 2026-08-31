#ifndef EDITORINTERACTION_H
#define EDITORINTERACTION_H

#include "Global/AppGlobal.h"
#include "Interface/EditorViewState.h"

namespace EditorInteraction {

    enum class Target { None, Tracks, PianoRoll, Parameters };

    enum class Command { Cut, Copy, Paste, SelectAll, DeleteSelection };

    [[nodiscard]] constexpr bool supportsCommand(const Target target,
                                                 const Command command) noexcept {
        if (target == Target::Parameters)
            return command == Command::DeleteSelection;
        return target == Target::Tracks || target == Target::PianoRoll;
    }

    [[nodiscard]] constexpr bool
        supportsCommand(const Target target, const Command command,
                        const EditorViewGlobal::PianoRollEditMode pianoRollEditMode) noexcept {
        if (!supportsCommand(target, command))
            return false;
        if (target != Target::PianoRoll || !EditorViewGlobal::isPitchEditMode(pianoRollEditMode))
            return true;
        return pianoRollEditMode == EditorViewGlobal::EditPitchAnchor &&
               command == Command::DeleteSelection;
    }

    [[nodiscard]] constexpr Target
        defaultTargetForPanel(const AppGlobal::PanelType panel) noexcept {
        switch (panel) {
            case AppGlobal::TracksEditor:
                return Target::Tracks;
            case AppGlobal::ClipEditor:
                return Target::PianoRoll;
            case AppGlobal::Generic:
                return Target::None;
        }
        return Target::None;
    }

} // namespace EditorInteraction

#endif // EDITORINTERACTION_H
