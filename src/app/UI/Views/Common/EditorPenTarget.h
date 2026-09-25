#ifndef EDITORPENTARGET_H
#define EDITORPENTARGET_H

#include "Interface/EditorViewState.h"

// What the stylus eraser (inverted tip, or the barrel button dragged) is
// allowed to do while a given tool is armed.
//
// The pen layer asks once per stroke, the moment the pen goes down, and keeps
// the answer for the whole stroke:
//
//   - Unsupported: the stroke is swallowed before it can reach the interaction
//     layer. Nothing happens at all, which is what "this tool has nothing to
//     erase" has to mean: letting the eraser through would make it perform the
//     tool's normal action (splitting a note, moving an anchor...).
//   - EraseNote / EraseParam: the stroke becomes an ordinary left-button
//     stroke that carries EditorPointer's erase intent, and the view routes it
//     to the erase path of the tool it is sitting on.
//
// Answering "Unsupported" is also what keeps the hover eraser cursor honest:
// a tool that cannot erase never advertises one.
enum class EditorPenEraser {
    Unsupported,
    // Notes: piano roll's note selection and note erase tools.
    EraseNote,
    // A parameter curve: the piano roll's pitch tools, and the parameter
    // editor's draw/erase/trace tools.
    EraseParam,
};

// What a view must provide for EditorPenController to drive it. Implemented by
// the same views as EditorTouchTarget (the legacy TimeGraphicsView family and
// the two RHI editor widgets), so there is exactly one policy table per
// backend.
class EditorPenTarget {
public:
    virtual ~EditorPenTarget();

    [[nodiscard]] virtual EditorPenEraser penEraserAction() const = 0;

    // Bracket an erase stroke. Views use it to arm whatever the armed tool
    // needs for a plain left-button stroke to erase: switching a handler,
    // forcing the pitch editor or the parameter foreground into erase mode.
    // Never called for EditorPenEraser::Unsupported, and always paired with
    // endPenEraserStroke().
    virtual void beginPenEraserStroke() {
    }
    virtual void endPenEraserStroke() {
    }
};

// The per-view policy tables, as pure functions so src/tests/TestPenInput can
// pin every mapping down without building a view.
namespace EditorPenPolicy {

    // Mappings that follow the tool's own purpose; see the strategy table in
    // docs/design/touch-and-pen-input-design.md §5.4-6.
    [[nodiscard]] EditorPenEraser pianoRoll(EditorViewGlobal::PianoRollEditMode mode);
    [[nodiscard]] EditorPenEraser parameterEditor(EditorViewGlobal::ParameterEditMode mode);
    // The arrangement canvas has no tool of its own and nothing to erase.
    [[nodiscard]] EditorPenEraser arrangement();

} // namespace EditorPenPolicy

#endif // EDITORPENTARGET_H
