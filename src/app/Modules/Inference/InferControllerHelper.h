#ifndef INFERCONTROLLERHELPER_H
#define INFERCONTROLLERHELPER_H

#include <lite/ProjectModel/AppModel/Params.h>
#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include "Models/PhonemeNameResult.h"
#include "Models/PronunciationFetchResult.h"
#include <lite/ProjectModel/AppModel/SingerIdentifier.h>
#include "Tasks/InferAcousticTask.h"
#include "Tasks/InferDurationTask.h"
#include "Tasks/InferPitchTask.h"
#include "Tasks/InferVarianceTask.h"

#include <QList>

class InferParamCurve;
class InferPiece;
class Note;
class SingingClip;
class InferInputNote;
class InferInputBase;
class Timeline;

using DurInput = InferDurationTask::InferDurInput;
using PitchInput = InferPitchTask::InferPitchInput;
using VarianceInput = InferVarianceTask::InferVarianceInput;
using AcousticInput = InferAcousticTask::InferAcousticInput;

namespace InferControllerHelper {
    struct ParamUpdate {
        DrawCurve original;
        DrawCurve input;
    };

    struct ParamInputUpdate {
        InferPiece *piece = nullptr;
        DrawCurve input;
    };

    QList<InferInputNote> buildInferInputNotes(const QList<Note *> &notes);
    InferInputBase buildInferBaseInput(const InferPiece &piece, const SingerIdentifier &identifier);
    DurInput buildInferDurInput(const InferPiece &piece, const SingerIdentifier &identifier);
    PitchInput buildInferPitchInput(const InferPiece &piece, const SingerIdentifier &identifier);
    VarianceInput buildInferVarianceInput(const InferPiece &piece,
                                          const SingerIdentifier &identifier);
    AcousticInput buildInferAcousticInput(const InferPiece &piece,
                                          const SingerIdentifier &identifier);
    QString buildSemanticSignature(const QString &taskType, const InferPiece &piece,
                                   const SingerIdentifier &identifier);

    // 查找由于编辑某个参数导致需要重新推理依赖参数的分段
    QList<ParamInputUpdate> buildParamInputUpdates(ParamInfo::Name name, SingingClip &clip,
                                                   const Timeline &timeline);
    QList<InferPiece *> getParamDirtyPiecesAndUpdateInput(ParamInfo::Name name, SingingClip &clip,
                                                          const Timeline &timeline);

    // Update original param methods
    void updatePronunciation(const QList<Note *> &notes,
                             const QList<PronunciationFetchResult> &args, SingingClip &clip);
    void updatePhoneName(const QList<Note *> &notes, const QList<PhonemeNameResult> &args,
                         SingingClip &clip);
    QList<QList<int>> collectPhoneOffsetsForStorage(const QList<Note *> &notes,
                                                    const QList<InferInputNote> &args,
                                                    const SingingClip &clip,
                                                    const Timeline &timeline);
    void updatePhoneOffset(const QList<Note *> &notes, const QList<InferInputNote> &args,
                           SingingClip &clip, const Timeline &timeline);
    ParamUpdate buildParamUpdate(ParamInfo::Name name, const InferParamCurve &taskResult,
                                 InferPiece &piece, int scale = 1000, int smoothKernelSize = -1);
    void updateParam(ParamInfo::Name name, const InferParamCurve &taskResult, InferPiece &piece,
                     int scale = 1000, int smoothKernelSize = -1);
    void updatePitch(const InferParamCurve &taskResult, InferPiece &piece,
                     int smoothKernelSize = -1);
    void updateVariance(const InferVarianceTask::InferVarianceResult &taskResult,
                        InferPiece &piece);
    void updateAcoustic(const QString &taskResult, InferPiece &piece);
    void updateAllOriginalParam(SingingClip &clip);

    // Reset original param methods
    void resetPhoneOffset(const QList<Note *> &notes, InferPiece &piece, bool cascadeReset = true);
    void resetParam(ParamInfo::Name name, InferPiece &piece);
    void resetPitch(InferPiece &piece, bool cascadeReset = true);
    void resetVariance(InferPiece &piece, bool cascadeReset = true);
    void resetAcoustic(InferPiece &piece);
};

#endif // INFERCONTROLLERHELPER_H
