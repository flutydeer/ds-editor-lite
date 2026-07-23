//
// Created by fluty on 24-10-5.
//

#ifndef INFERINPUTBASE_H
#define INFERINPUTBASE_H

#include "Model/AppModel/Timeline.h"
#include "InferenceTaskContext.h"
#include "Model/Infer/InferSpeakerMix.h"
#include "InferInputNote.h"
#include "Model/AppModel/SingerIdentifier.h"

#include <QList>
#include <QJsonArray>
#include <QJsonObject>
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
    double depth = 1.0;
    int pitchSmoothKernelSize = -1;

    [[nodiscard]] QJsonObject semanticObject(const QString &taskType) const;
    [[nodiscard]] QString semanticSignature(const QString &taskType,
                                            const QJsonObject &extra = {}) const;

    [[nodiscard]] InferenceTaskContext toInferenceTaskContext(const QString &taskType) const {
        InferenceTaskContext context;
        context.taskType = taskType;
        context.clipId = clipId;
        context.pieceId = pieceId;
        context.clipRevision = clipRevision;
        context.singer = identifier;
        context.speaker = speaker;
        context.speakerMixSignature = speakerMix.signature();
        context.pitchSmoothKernelSize = pitchSmoothKernelSize;
        context.noteIds.reserve(notes.count());
        for (const auto &note : notes)
            context.noteIds.append(note.id);
        return context;
    }

protected:
    [[nodiscard]] static QJsonArray doubleArray(const QList<double> &values);
};

#endif // INFERINPUTBASE_H
