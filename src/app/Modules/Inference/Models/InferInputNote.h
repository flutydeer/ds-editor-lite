#ifndef INFERINPUTNOTE_H
#define INFERINPUTNOTE_H

#include <lite/ProjectModel/AppModel/Phonemes.h>
#include <QList>
#include <QSet>

#include "Modules/Inference/Utils/ReservedPhonemes.h"

class Note;

class InferInputNote {
public:
    /// A note whose lyric is in \a reservedPhonemes is a rest; see ReservedPhonemes::of().
    explicit InferInputNote(const Note &note,
                            const QSet<QString> &reservedPhonemes = ReservedPhonemes::forced());

    int id = -1;
    int start = 0;
    int length = 0;
    int key = -1;
    bool isRest = false;
    bool isSlur = false;
    bool isSyllabification = false;
    QString languageDictId;
    QList<PhonemeName> phonemeNames;
    QList<int> phonemeOffsets;

    friend bool operator==(const InferInputNote &lhs, const InferInputNote &rhs);
    friend bool operator!=(const InferInputNote &lhs, const InferInputNote &rhs);
};

#endif // INFERINPUTNOTE_H
