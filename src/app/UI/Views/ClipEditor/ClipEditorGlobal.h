//
// Created by fluty on 2024/1/26.
//

#ifndef PIANOROLLGLOBAL_H
#define PIANOROLLGLOBAL_H

namespace ClipEditorGlobal {
    constexpr int pixelsPerQuarterNote = 96;
    constexpr double noteHeight = 24;
    constexpr int timelineViewHeight = 24;
    constexpr int pianoKeyboardWidth = 80;

    enum PianoRollEditMode { Select, DrawNote, DrawPitch, EditPitchAnchor };
}

#endif // PIANOROLLGLOBAL_H
