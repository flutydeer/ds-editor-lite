//
// Created by FlutyDeer on 2026/6/7.
//

#ifndef DS_EDITOR_LITE_INFERENCETASKCONTEXT_H
#define DS_EDITOR_LITE_INFERENCETASKCONTEXT_H

#include "SingerIdentifier.h"

#include <QList>
#include <QString>
#include <QtGlobal>

class InferPiece;
class Note;
class SingingClip;

struct InferenceTaskContext {
    QString taskType;
    int taskId = -1;
    int clipId = -1;
    int pieceId = -1;
    quint64 clipRevision = 0;
    QList<int> noteIds;
    SingerIdentifier singer;
    QString speaker;
};

struct InferenceTaskResolution {
    SingingClip *clip = nullptr;
    InferPiece *piece = nullptr;
    QList<Note *> notes;
    QString dropReason;
};

#endif // DS_EDITOR_LITE_INFERENCETASKCONTEXT_H
