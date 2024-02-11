//
// Created by fluty on 2024/1/26.
//

#ifndef PIANOROLLGLOBAL_H
#define PIANOROLLGLOBAL_H

namespace PianoRollGlobal {
    const int pixelsPerQuarterNote = 96;
    const double noteHeight = 24;
    const int timelineViewHeight = 24;

    enum PianoRollEditMode { Select, DrawNote, DrawPitch, EditPitchAnchor };
    enum NoteEditMode { NotePosition, Length, StartAndLength };
}

#endif //PIANOROLLGLOBAL_H
