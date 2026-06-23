//
// Created by fluty on 24-10-5.
//

#ifndef INFERINPUTBASE_H
#define INFERINPUTBASE_H

#include "Model/AppModel/Timeline.h"
#include "InferenceTaskContext.h"
#include "InferSpeakerMix.h"
#include "InferInputNote.h"
#include "SingerIdentifier.h"

#include <QList>
#include <QString>

class InferInputBase {
public:
    int clipId = -1;
    int pieceId = -1;
    quint64 clipRevision = 0;

    double headAvailableLengthMs = 0;
    double paddingStartMs = 0;
    double paddingEndMs = 0;

    Timeline timeline;
    QList<InferInputNote> notes;

    QString speaker;
    InferSpeakerMix speakerMix;
    SingerIdentifier identifier;
    int steps = -1;

    [[nodiscard]] InferenceTaskContext toInferenceTaskContext(const QString &taskType) const {
        InferenceTaskContext context;
        context.taskType = taskType;
        context.clipId = clipId;
        context.pieceId = pieceId;
        context.clipRevision = clipRevision;
        context.singer = identifier;
        context.speaker = speaker;
        context.speakerMixSignature = speakerMix.signature();
        context.noteIds.reserve(notes.count());
        for (const auto &note : notes)
            context.noteIds.append(note.id);
        return context;
    }
};

#endif // INFERINPUTBASE_H
