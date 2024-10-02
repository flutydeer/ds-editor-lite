//
// Created by fluty on 24-9-26.
//

#include "InferDurPitNote.h"

#include "Model/AppModel/Note.h"

InferDurPitNote::InferDurPitNote(const Note &note) {
    id = note.id();
    start = note.rStart();
    length = note.length();
    key = note.keyIndex();
    isRest = note.lyric() == "SP" || note.lyric() == "AP";
    isSlur = note.isSlur();
    aheadNames = note.phonemeNameInfo().ahead.result();
    normalNames = note.phonemeNameInfo().normal.result();
    aheadOffsets = note.phonemeOffsetInfo().ahead.result();
    normalOffsets = note.phonemeOffsetInfo().normal.result();
}