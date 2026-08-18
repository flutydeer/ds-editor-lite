#include "InferInputNote.h"

#include <lite/ProjectModel/AppModel/Note.h>

InferInputNote::InferInputNote(const Note &note) {
    id = note.id();
    start = note.localStart();
    length = note.length();
    key = note.keyIndex();
    isRest = note.lyric() == "SP" || note.lyric() == "AP";
    isSlur = note.isSlur();
    isSyllabification = note.isSyllabification();
    languageDictId = note.effectiveLanguage();
    phonemeNames = note.phonemeNameSeq().result();
    phonemeOffsets = note.phonemeOffsetSeq().result();
}

bool operator==(const InferInputNote &lhs, const InferInputNote &rhs) {
    const bool idEqual = lhs.id == rhs.id;
    const bool startEqual = lhs.start == rhs.start;
    const bool lengthEqual = lhs.length == rhs.length;
    const bool keyEqual = lhs.key == rhs.key;
    const bool isRestEqual = lhs.isRest == rhs.isRest;
    const bool isSlurEqual = lhs.isSlur == rhs.isSlur;
    const bool isSyllabificationEqual = lhs.isSyllabification == rhs.isSyllabification;
    const bool phonemeNamesEqual = lhs.phonemeNames == rhs.phonemeNames;
    const bool phonemeOffsetsEqual = lhs.phonemeOffsets == rhs.phonemeOffsets;
    return idEqual && startEqual && lengthEqual && keyEqual && isRestEqual && isSlurEqual &&
           isSyllabificationEqual && phonemeNamesEqual && phonemeOffsetsEqual;
}

bool operator!=(const InferInputNote &lhs, const InferInputNote &rhs) {
    return !(lhs == rhs);
}
