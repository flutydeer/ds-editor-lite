//
// Created by fluty on 24-9-26.
//

#ifndef INFERDURNOTE_H
#define INFERDURNOTE_H

#include <QList>

class Note;
class InferDurNote {
public:
    explicit InferDurNote(const Note &note);

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
};

#endif //INFERDURNOTE_H
