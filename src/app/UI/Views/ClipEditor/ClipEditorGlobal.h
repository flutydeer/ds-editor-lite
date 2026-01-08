//
// Created by fluty on 2024/1/26.
//

#ifndef PIANOROLLGLOBAL_H
#define PIANOROLLGLOBAL_H

namespace ClipEditorGlobal {
    constexpr int pixelsPerQuarterNote = 96;
    constexpr double noteHeight = 24;
    constexpr int timelineViewHeight = 32;  // Increased for loop region display
    constexpr int pianoKeyboardWidth = 64;

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
}

#endif // PIANOROLLGLOBAL_H
