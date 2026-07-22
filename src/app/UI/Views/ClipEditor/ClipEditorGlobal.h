//
// Created by fluty on 2024/1/26.
//

#ifndef PIANOROLLGLOBAL_H
#define PIANOROLLGLOBAL_H

#include "Interface/EditorViewState.h"

namespace ClipEditorGlobal {
    constexpr int pixelsPerQuarterNote = 96;
    constexpr double noteHeight = 24;
    constexpr int timelineViewHeight = 32;  // Increased for loop region display
    constexpr int pianoKeyboardWidth = 64;

    using PianoRollEditMode = EditorViewGlobal::PianoRollEditMode;
    inline constexpr auto Select = EditorViewGlobal::Select;
    inline constexpr auto IntervalSelect = EditorViewGlobal::IntervalSelect;
    inline constexpr auto DrawNote = EditorViewGlobal::DrawNote;
    inline constexpr auto EraseNote = EditorViewGlobal::EraseNote;
    inline constexpr auto SplitNote = EditorViewGlobal::SplitNote;
    inline constexpr auto DrawPitch = EditorViewGlobal::DrawPitch;
    inline constexpr auto EditPitchAnchor = EditorViewGlobal::EditPitchAnchor;
    inline constexpr auto ErasePitch = EditorViewGlobal::ErasePitch;
    inline constexpr auto FreezePitch = EditorViewGlobal::FreezePitch;
}

#endif // PIANOROLLGLOBAL_H
