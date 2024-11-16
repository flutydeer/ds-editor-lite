//
// Created by fluty on 24-9-26.
//

#include "InferInputNote.h"

#include "Model/AppModel/Note.h"

InferInputNote::InferInputNote(const Note &note) {
    id = note.id();
    start = note.localStart();
    length = note.length();
    key = note.keyIndex();
    isRest = note.lyric() == "SP" || note.lyric() == "AP";
    isSlur = note.isSlur();
    // TODO: language dict id form singer info
    languageDictId = note.language();
    aheadNames = note.phonemeNameInfo().ahead.result();
    normalNames = note.phonemeNameInfo().normal.result();
    aheadOffsets = note.phonemeOffsetInfo().ahead.result();
    normalOffsets = note.phonemeOffsetInfo().normal.result();
}

bool operator==(const InferInputNote &lhs, const InferInputNote &rhs) {
    bool idEqual = lhs.id == rhs.id;
    bool startEqual = lhs.start == rhs.start;
    bool lengthEqual = lhs.length == rhs.length;
    bool keyEqual = lhs.key == rhs.key;
    bool isRestEqual = lhs.isRest == rhs.isRest;
    bool isSlurEqual = lhs.isSlur == rhs.isSlur;
    bool aheadNamesEqual = lhs.aheadNames == rhs.aheadNames;
    bool normalNamesEqual = lhs.normalNames == rhs.normalNames;
    bool aheadOffsetsEqual = lhs.aheadOffsets == rhs.aheadOffsets;
    bool normalOffsetsEqual = lhs.normalOffsets == rhs.normalOffsets;
    return idEqual && startEqual && lengthEqual && keyEqual && isRestEqual && isSlurEqual &&
           aheadNamesEqual && normalNamesEqual && aheadOffsetsEqual && normalOffsetsEqual;
}

bool operator!=(const InferInputNote &lhs, const InferInputNote &rhs) {
    return !(lhs == rhs);
}