//
// Created by fluty on 24-9-26.
//

#ifndef INFERINPUTNOTE_H
#define INFERINPUTNOTE_H

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
    QStringList aheadNames;
    QStringList normalNames;

    QList<int> aheadOffsets;
    QList<int> normalOffsets;

    friend bool operator==(const InferInputNote &lhs, const InferInputNote &rhs);
    friend bool operator!=(const InferInputNote &lhs, const InferInputNote &rhs);
};

#endif //INFERINPUTNOTE_H
