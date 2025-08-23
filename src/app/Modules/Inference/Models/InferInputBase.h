//
// Created by fluty on 24-10-5.
//

#ifndef INFERINPUTBASE_H
#define INFERINPUTBASE_H

#define INFER_INPUT_COMMON_MEMBERS                                                                 \
    int clipId = -1;                                                                               \
    int pieceId = -1;                                                                              \
    QList<InferInputNote> notes;                                                                   \
    SingerIdentifier identifier;                                                                   \
    QString speaker;                                                                               \
    double tempo;


#endif // INFERINPUTBASE_H
