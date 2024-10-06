//
// Created by fluty on 24-9-6.
//

#ifndef INFERCONTROLLERHELPER_H
#define INFERCONTROLLERHELPER_H

#include "Model/AppModel/Params.h"
#include "Model/Inference/InferParamCurve.h"
#include "Modules/Inference/InferVarianceTask.h"

#include <QList>

class InferPiece;
class Note;
class SingingClip;
class InferInputNote;
class PhonemeNameResult;

class InferControllerHelper {
public:
    static QList<InferInputNote> buildInferInputNotes(const QList<Note *> &notes);

    // Update original param methods
    static void updatePronunciation(const QList<Note *> &notes, const QList<QString> &args,
                                    SingingClip &clip);
    static void updatePhoneName(const QList<Note *> &notes, const QList<PhonemeNameResult> &args,
                                SingingClip &clip);
    static void updatePhoneOffset(const QList<Note *> &notes, const QList<InferInputNote> &args,
                                  SingingClip &clip);
    static void updateParam(ParamInfo::Name name, const InferParamCurve &taskResult,
                            InferPiece &piece);
    static void updatePitch(const InferParamCurve &taskResult, InferPiece &piece);
    static void updateVariance(const InferVarianceTask::InferVarianceResult &taskResult,
                               InferPiece &piece);

    // Reset original param methods
    // TODO: 可能也要重置依赖的参数
    static void resetPhoneOffset(const QList<Note *> &notes, SingingClip &clip);
    static void resetParam(ParamInfo::Name name, InferPiece &piece);
    static void resetPitch(InferPiece &piece);
    static void resetVariance(InferPiece &piece);
};

#endif // INFERCONTROLLERHELPER_H
