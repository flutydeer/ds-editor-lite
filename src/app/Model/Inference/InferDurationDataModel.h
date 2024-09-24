//
// Created by fluty on 24-9-10.
//

#ifndef INFERDURATIONDATAMODEL_H
#define INFERDURATIONDATAMODEL_H

#include <QList>

class InferDurNote {
public:
    int id = -1;
    int start = 0;
    int length = 0;
    QStringList aheadNames;
    QStringList normalNames;

    QList<int> aheadOffsets;
    QList<int> normalOffsets;
};

// class InferDurationInput {
// public:
//     QList<InferDurNote> notes;
// };
//
// class InferDurationResult {
// public:
//     QList<InferDurNote> notes;
// };


#endif // INFERDURATIONDATAMODEL_H
