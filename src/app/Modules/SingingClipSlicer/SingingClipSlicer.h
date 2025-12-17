//
// Created by FlutyDeer on 2025/11/27.
//

#ifndef DS_EDITOR_LITE_SINGINGCLIPSLICER_H
#define DS_EDITOR_LITE_SINGINGCLIPSLICER_H

#include "Models/SliceResult.h"

class Note;
class Timeline;

namespace SingingClipSlicer {
    using NoteList = QList<Note *>;

    SliceResult slice(const Timeline &timeline, const NoteList &source);
    // SliceResult simpleSlice(const NoteList &source, double threshold = 200);
};


#endif //DS_EDITOR_LITE_SINGINGCLIPSLICER_H