//
// Created by fluty on 24-9-6.
//

#include "InferControllerHelper.h"

#include "Model/AppModel/DrawCurve.h"
#include "Model/AppModel/Note.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppModel/InferPiece.h"
#include "Models/InferInputNote.h"
#include "Utils/Linq.h"

#include <QDebug>

namespace InferControllerHelper {
    QList<InferInputNote> buildInferInputNotes(const QList<Note *> &notes) {
        QList<InferInputNote> list;
        for (const auto note : notes)
            list.append(InferInputNote(*note));
        return list;
    }

    void updatePronunciation(const QList<Note *> &notes, const QList<QString> &args,
                             SingingClip &clip) {
        if (notes.count() != args.count()) {
            qFatal() << "updateNotesPronunciation() note count != args count:" << notes.count()
                     << args.count();
            return;
        }
        int i = 0;
        for (const auto note : notes) {
            note->setPronunciation(Note::Original, args[i]);
            i++;
        }
        clip.notifyNoteChanged(SingingClip::OriginalWordPropertyChange, notes);
    }

    void updatePhoneName(const QList<Note *> &notes, const QList<PhonemeNameResult> &args,
                         SingingClip &clip) {
        if (notes.count() != args.count()) {
            qFatal() << "updateNotesPhonemeName() note count != args count:" << notes.count()
                     << args.count();
            return;
        }
        int i = 0;
        for (const auto note : notes) {
            note->setPhonemeNameInfo(Phonemes::Ahead, Note::Original, args[i].aheadNames);
            note->setPhonemeNameInfo(Phonemes::Normal, Note::Original, args[i].normalNames);
            i++;
        }
        clip.notifyNoteChanged(SingingClip::OriginalWordPropertyChange, notes);
    }

    void updatePhoneOffset(const QList<Note *> &notes, const QList<InferInputNote> &args,
                           SingingClip &clip) {
        if (notes.count() != args.count()) {
            qFatal() << "updateNotesPhonemeName() note count != args count:" << notes.count()
                     << args.count();
            return;
        }
        int i = 0;
        for (const auto note : notes) {
            note->setPhonemeOffsetInfo(Phonemes::Ahead, Note::Original, args[i].aheadOffsets);
            note->setPhonemeOffsetInfo(Phonemes::Normal, Note::Original, args[i].normalOffsets);
            i++;
        }
        clip.notifyNoteChanged(SingingClip::OriginalWordPropertyChange, notes);
    }

    void updateParam(const ParamInfo::Name name, const InferParamCurve &taskResult,
                     InferPiece &piece, int scale) {
        // 将推理结果保存到分段内部
        DrawCurve resultCurve;
        resultCurve.start = piece.realStartTick();
        resultCurve.setValues(
            Linq::selectMany(taskResult.values, L_PRED(v, static_cast<int>(v * scale))));
        piece.setCurve(name, resultCurve);
        piece.clip->updateOriginalParam(name);
    }

    void updatePitch(const InferParamCurve &taskResult, InferPiece &piece) {
        updateParam(ParamInfo::Pitch, taskResult, piece, 100);
    }

    void updateVariance(const InferVarianceTask::InferVarianceResult &taskResult,
                        InferPiece &piece) {
        updateParam(ParamInfo::Breathiness, taskResult.breathiness, piece);
        updateParam(ParamInfo::Tension, taskResult.tension, piece);
        updateParam(ParamInfo::Voicing, taskResult.voicing, piece);
        updateParam(ParamInfo::Energy, taskResult.energy, piece);
    }

    void updateAcoustic(const QString &taskResult, InferPiece &piece) {
        qInfo() << "Saved audio to" << taskResult;
        piece.acousticInferStatus = Success;
    }

    void resetPhoneOffset(const QList<Note *> &notes, InferPiece &piece, bool cascadeReset) {
        if (cascadeReset)
            resetPitch(piece);
        for (const auto note : notes) {
            note->setPhonemeOffsetInfo(Phonemes::Ahead, Note::Original, {});
            note->setPhonemeOffsetInfo(Phonemes::Normal, Note::Original, {});
        }
        piece.clip->notifyNoteChanged(SingingClip::OriginalWordPropertyChange, notes);
    }

    void resetParam(ParamInfo::Name name, InferPiece &piece) {
        DrawCurve emptyCurve;
        piece.setCurve(name, emptyCurve);
        piece.clip->updateOriginalParam(name);
    }

    void resetPitch(InferPiece &piece, bool cascadeReset) {
        if (cascadeReset)
            resetVariance(piece);
        resetParam(ParamInfo::Pitch, piece);
    }

    void resetVariance(InferPiece &piece, bool cascadeReset) {
        if (cascadeReset)
            resetAcoustic(piece);
        resetParam(ParamInfo::Breathiness, piece);
        resetParam(ParamInfo::Tension, piece);
        resetParam(ParamInfo::Voicing, piece);
        resetParam(ParamInfo::Energy, piece);
    }

    void resetAcoustic(InferPiece &piece) {
        piece.audioPath = QString();
        // piece.acousticInferStatus = Pending;
    }
}