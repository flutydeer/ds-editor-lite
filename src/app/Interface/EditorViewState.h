#ifndef EDITORVIEWSTATE_H
#define EDITORVIEWSTATE_H

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
        FreezePitch
    };

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
    QString bottomPanelPageId = QStringLiteral("ClipEditor");

    bool operator==(const EditorLayoutState &) const = default;
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

    bool operator==(const EditorViewState &) const = default;
};

#endif // EDITORVIEWSTATE_H
