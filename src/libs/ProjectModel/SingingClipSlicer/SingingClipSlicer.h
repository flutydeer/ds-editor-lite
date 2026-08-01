#ifndef DS_EDITOR_LITE_SINGINGCLIPSLICER_H
#define DS_EDITOR_LITE_SINGINGCLIPSLICER_H

#include <lite/ProjectModel/SingingClipSlicer/Models/SliceResult.h>

class Note;
class Timeline;

namespace SingingClipSlicer {
    using NoteList = QList<Note *>;

    SliceResult slice(const Timeline &timeline, const NoteList &source);
};


#endif //DS_EDITOR_LITE_SINGINGCLIPSLICER_H