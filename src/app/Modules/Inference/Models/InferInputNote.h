//
// Created by fluty on 24-9-26.
//

#ifndef INFERINPUTNOTE_H
#define INFERINPUTNOTE_H

#include "Model/AppModel/Phonemes.h"
#include <QList>

class Note;

class InferInputNote {
public:
    explicit InferInputNote(const Note &note);

    int id = -1;
    int start = 0;
    int length = 0;
    int key = -1;
    bool isRest = false;
    bool isSlur = false;
    QString languageDictId;
    QList<PhonemeName> phonemeNames;
    QList<int> phonemeOffsets;

    friend bool operator==(const InferInputNote &lhs, const InferInputNote &rhs);
    friend bool operator!=(const InferInputNote &lhs, const InferInputNote &rhs);
};

#endif // INFERINPUTNOTE_H
