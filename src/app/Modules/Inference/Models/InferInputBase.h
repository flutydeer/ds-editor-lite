#ifndef INFERINPUTBASE_H
#define INFERINPUTBASE_H

#include <lite/MusicBase/Timeline.h>
#include "InferenceTaskContext.h"
#include "InferParamCurve.h"
#include <lite/ProjectModel/InferenceData/InferSpeakerMix.h>
#include "InferInputNote.h"
#include <lite/ProjectModel/AppModel/SingerIdentifier.h>

#include <QList>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

class InferInputBase {
public:
    int clipId = -1;
    int pieceId = -1;
    quint64 clipRevision = 0;

    // Project-absolute tick coordinates. Notes and parameter curves remain clip-local.
    int clipStartTick = 0;
    int pieceStartTick = 0;
    int pieceEndTick = 0;

    double headAvailableLengthMs = 0;
    double paddingStartMs = 0;
    double paddingEndMs = 0;
    int minimumFirstOffsetMs = 0;
    double requiredHeadLengthMs = 0;
    double maximumHeadLengthMs = 0;

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
    [[nodiscard]] QList<double> resampleCurveToFrames(const InferParamCurve &curve, int frames,
                                                      double intervalSeconds = 0.01,
                                                      double emptyValue = 0.0) const;
    [[nodiscard]] InferParamCurve resampleFramesToCurve(const QList<double> &values,
                                                        double intervalSeconds) const;

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
    [[nodiscard]] static QJsonObject paramCurveObject(const InferParamCurve &curve);

private:
    [[nodiscard]] QList<double> curveSampleSeconds(const InferParamCurve &curve) const;
    [[nodiscard]] QList<double> frameSampleSeconds(int frames, double intervalSeconds) const;
};

#endif // INFERINPUTBASE_H
