//
// Created by FlutyDeer on 2025/11/27.
//

#ifndef DS_EDITOR_LITE_SLICERESULT_H
#define DS_EDITOR_LITE_SLICERESULT_H

#include <QList>

class Note;
using NoteList = QList<Note *>;

class Segment {
public:
    double headAvailableLengthMs = 0;
    double paddingStartMs = 0;
    double paddingEndMs = 0;
    QList<Note *> notes;
};

class SliceResult {
public:
    QList<Segment> segments;
};

#endif //DS_EDITOR_LITE_SLICERESULT_H