//
// Created by fluty on 24-10-5.
//

#ifndef INFERINPUTBASE_H
#define INFERINPUTBASE_H

#include "Model/AppModel/Timeline.h"
#include "SingerIdentifier.h"

class InferInputNote;

class InferInputBase {
public:
    int clipId = -1;
    int pieceId = -1;

    double headAvailableLengthMs = 0;
    double paddingStartMs = 0;
    double paddingEndMs = 0;

    Timeline timeline;
    QList<InferInputNote> notes;

    QString speaker;
    SingerIdentifier identifier;
    int steps = -1;
};

#endif // INFERINPUTBASE_H
