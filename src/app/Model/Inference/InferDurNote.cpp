//
// Created by fluty on 24-9-26.
//

#include "InferDurNote.h"

#include "Model/AppModel/Note.h"

InferDurNote::InferDurNote(const Note &note) {
    id = note.id();
    start = note.rStart();
    length = note.length();
    key = note.keyIndex();
    isRest = note.lyric() == "SP" || note.lyric() == "AP";
    isSlur = note.isSlur();
    aheadNames = note.phonemeNameInfo().ahead.result();
    normalNames = note.phonemeNameInfo().normal.result();
}