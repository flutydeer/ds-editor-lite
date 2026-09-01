#ifndef EDITORVIEWSTATE_H
#define EDITORVIEWSTATE_H

#include <lite/ProjectModel/AppModel/Params.h>

#include <QString>

namespace EditorViewGlobal {

    enum PianoRollEditMode {
        Select,
        IntervalSelect,
        DrawNote,
        EraseNote,
        SplitNote,
        DrawPitch,
        EditPitchAnchor,
        ErasePitch,
        BakePitch
    };

    enum class Region {
        None,
        TrackPanel,
        PianoRoll,
        Parameters,
    };

    enum class ParameterEditMode {
        Draw,
        Erase,
        Bake,
        Anchor,
    };

    [[nodiscard]] constexpr bool isPitchEditMode(const PianoRollEditMode mode) noexcept {
        return mode == DrawPitch || mode == EditPitchAnchor || mode == ErasePitch ||
               mode == BakePitch;
    }

} // namespace EditorViewGlobal

struct TrackPanelViewState {
    double centerTick = 0;
    double centerTrackIndex = 0;
    double horizontalScale = 1;
    double verticalScale = 1;

    bool operator==(const TrackPanelViewState &) const = default;
};

struct EditorLayoutState {
    bool trackPanelVisible = true;
    bool bottomPanelVisible = true;
    bool pianoRollVisible = true;
    bool parametersVisible = true;
    QString bottomPanelPageId = QStringLiteral("ClipEditor");
    EditorViewGlobal::Region activeRegion = EditorViewGlobal::Region::TrackPanel;
    EditorViewGlobal::Region focusedRegion = EditorViewGlobal::Region::None;

    bool operator==(const EditorLayoutState &) const = default;
};

struct ParameterEditorViewState {
    ParamInfo::Name foreground = ParamInfo::Breathiness;
    ParamInfo::Name background = ParamInfo::Tension;
    EditorViewGlobal::ParameterEditMode editMode = EditorViewGlobal::ParameterEditMode::Draw;
    double centerRatio = 0.5;
    double verticalScale = 1.0;

    bool operator==(const ParameterEditorViewState &) const = default;
};

struct PianoRollViewState {
    double centerTick = 0;
    double centerKeyIndex = 60;
    double horizontalScale = 1;
    double verticalScale = 1;
    EditorViewGlobal::PianoRollEditMode editMode = EditorViewGlobal::Select;

    bool operator==(const PianoRollViewState &) const = default;
};

struct EditorViewState {
    TrackPanelViewState trackPanel;
    EditorLayoutState layout;
    PianoRollViewState pianoRoll;
    ParameterEditorViewState parameters;

    bool operator==(const EditorViewState &) const = default;
};

#endif // EDITORVIEWSTATE_H
