#include "EditorPenTarget.h"

EditorPenTarget::~EditorPenTarget() = default;

namespace EditorPenPolicy {

    EditorPenEraser pianoRoll(const EditorViewGlobal::PianoRollEditMode mode) {
        // The piano roll tool enum is unscoped, so its enumerators live in the
        // namespace rather than behind the type name.
        using namespace EditorViewGlobal;
        switch (mode) {
            // Note domain: both erasing tools are "erase the note under the
            // pen", which is exactly what EraseNoteHandler does. DrawNote is
            // not in the plan's strategy table; erasing notes is the only
            // meaning the eraser can have there, so it behaves like Select
            // instead of advertising nothing.
            case Select:
            case EraseNote:
            case DrawNote:
                return EditorPenEraser::EraseNote;
            // Pitch domain: the eraser clears the curve under the pen, the same
            // thing the erase mode of the pitch editor does.
            case DrawPitch:
            case ErasePitch:
            case TracePitch:
                return EditorPenEraser::EraseParam;
            // Nothing an eraser could sensibly do: splitting, anchor editing,
            // pitch modulation (a curve transform, not a draw) and interval
            // selection all get the stroke swallowed whole.
            case SplitNote:
            case EditPitchAnchor:
            case ModulatePitch:
            case IntervalSelect:
                return EditorPenEraser::Unsupported;
        }
        return EditorPenEraser::Unsupported;
    }

    EditorPenEraser parameterEditor(const EditorViewGlobal::ParameterEditMode mode) {
        using Mode = EditorViewGlobal::ParameterEditMode;
        switch (mode) {
            case Mode::Draw:
            case Mode::Erase:
            case Mode::Trace:
                return EditorPenEraser::EraseParam;
            // Curve transforms and anchor editing have no erase semantics.
            case Mode::Shape:
            case Mode::Scale:
            case Mode::Anchor:
                return EditorPenEraser::Unsupported;
        }
        return EditorPenEraser::Unsupported;
    }

    EditorPenEraser arrangement() {
        return EditorPenEraser::Unsupported;
    }

} // namespace EditorPenPolicy
