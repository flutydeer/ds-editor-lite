//
// Created by fluty on 24-9-6.
//

#ifndef INFERCONTROLLERHELPER_H
#define INFERCONTROLLERHELPER_H

#include "Model/AppModel/Params.h"
#include "Model/AppModel/SingingClip.h"
#include "Models/PhonemeNameResult.h"
#include "Modules/Inference/Models/SingerIdentifier.h"
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

using DurInput = InferDurationTask::InferDurInput;
using PitchInput = InferPitchTask::InferPitchInput;
using VarianceInput = InferVarianceTask::InferVarianceInput;
using AcousticInput = InferAcousticTask::InferAcousticInput;

namespace InferControllerHelper {
    QList<InferInputNote> buildInferInputNotes(const QList<Note *> &notes);
    DurInput buildInferDurInput(const InferPiece &piece, const SingerIdentifier &identifier);
    PitchInput buildInferPitchInput(const InferPiece &piece, const SingerIdentifier &identifier);
    VarianceInput buildInferVarianceInput(const InferPiece &piece,
                                          const SingerIdentifier &identifier);
    AcousticInput buildInderAcousticInput(const InferPiece &piece,
                                          const SingerIdentifier &identifier);

    // 查找由于编辑某个参数导致需要重新推理依赖参数的分段
    QList<InferPiece *> getParamDirtyPiecesAndUpdateInput(ParamInfo::Name name, SingingClip &clip);

    // Update original param methods
    void updatePronunciation(const QList<Note *> &notes, const QList<QString> &args,
                             SingingClip &clip);
    void updatePhoneName(const QList<Note *> &notes, const QList<PhonemeNameResult> &args,
                         SingingClip &clip);
    void updatePhoneOffset(const QList<Note *> &notes, const QList<InferInputNote> &args,
                           SingingClip &clip);
    void updateParam(ParamInfo::Name name, const InferParamCurve &taskResult, InferPiece &piece,
                     int scale = 1000);
    void updatePitch(const InferParamCurve &taskResult, InferPiece &piece);
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
