#ifndef EDITORINTERACTION_H
#define EDITORINTERACTION_H

#include "Global/AppGlobal.h"

namespace EditorInteraction {

    enum class Target { None, Tracks, PianoRoll, Parameters };

    enum class Command { Cut, Copy, Paste, SelectAll, DeleteSelection };

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
