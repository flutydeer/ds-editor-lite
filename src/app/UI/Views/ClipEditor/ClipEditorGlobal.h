//
// Created by fluty on 2024/1/26.
//

#ifndef PIANOROLLGLOBAL_H
#define PIANOROLLGLOBAL_H

namespace ClipEditorGlobal {
    constexpr int pixelsPerQuarterNote = 96;
    constexpr double noteHeight = 24;
    constexpr int timelineViewHeight = 24;
    constexpr int pianoKeyboardWidth = 64;

    enum PianoRollEditMode {
        Select,
        IntervalSelect,
        DrawNote,
        EraseNote,
        DrawPitch,
        EditPitchAnchor,
        ErasePitch,
        FreezePitch
    };
}

#endif // PIANOROLLGLOBAL_H
