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

// Editing hooks used by EditorPenController during a pen stroke.
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
    // Tear down an erase stroke the system interrupted (window deactivated,
    // widget hidden) without committing it: discard whatever the in-flight
    // stroke has done so far and un-arm everything beginPenEraserStroke() set.
    // The normal end of a stroke goes through endPenEraserStroke() instead,
    // and the pen leaving the digitizer range mid-stroke releases the stroke
    // through EditorPenController::cancel(), so an implementation never has to
    // guess which ending it is serving.
    virtual void abortPenEraseStroke() {
    }
};

// Resolve the pen eraser action for the active editor tool.
namespace EditorPenPolicy {

    // See the pen eraser strategy table in docs/design/touch-and-pen-input-design.md.
    [[nodiscard]] EditorPenEraser pianoRoll(EditorViewGlobal::PianoRollEditMode mode);
    [[nodiscard]] EditorPenEraser parameterEditor(EditorViewGlobal::ParameterEditMode mode);
    // The arrangement canvas has no tool of its own and nothing to erase.
    [[nodiscard]] EditorPenEraser arrangement();

} // namespace EditorPenPolicy

#endif // EDITORPENTARGET_H
