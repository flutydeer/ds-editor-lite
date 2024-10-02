//
// Created by fluty on 24-9-6.
//

#ifndef ORIGINALPARAMUTILS_H
#define ORIGINALPARAMUTILS_H

#include "Model/AppModel/Params.h"
#include "Model/Inference/InferParamCurve.h"

#include <QList>


class InferPiece;
class Note;
class SingingClip;
class InferDurPitNote;
class PhonemeNameResult;

class OriginalParamUtils {
public:
    static void updatePronunciation(const QList<Note *> &notes, const QList<QString> &args,
                                    SingingClip *clip);
    static void updatePhoneName(const QList<Note *> &notes, const QList<PhonemeNameResult> &args,
                                SingingClip *clip);
    static void updatePhoneOffset(const QList<Note *> &notes, const QList<InferDurPitNote> &args,
                                  SingingClip *clip);
    static void resetPhoneOffset(const QList<Note *> &notes, SingingClip *clip);
    static void updateParam(ParamInfo::Name name, const InferParamCurve &curve, SingingClip *clip,
                            InferPiece *piece);
};

#endif // ORIGINALPARAMUTILS_H
