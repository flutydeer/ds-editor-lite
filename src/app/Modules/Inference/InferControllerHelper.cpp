//
// Created by fluty on 24-9-6.
//

#include "InferControllerHelper.h"

#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/DrawCurve.h"
#include "Model/AppModel/Note.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppModel/InferPiece.h"
#include "Models/InferInputNote.h"
#include "Utils/AppModelUtils.h"
#include "Utils/Linq.h"
#include "Utils/MathUtils.h"

#include <QDebug>

namespace InferControllerHelper {
    QList<InferInputNote> buildInferInputNotes(const QList<Note *> &notes) {
        QList<InferInputNote> list;
        for (const auto note : notes)
            list.append(InferInputNote(*note));
        return list;
    }

    InferVarianceTask::InferVarianceInput buildInferVarianceInput(const InferPiece &piece,
                                                                  const QString &configPath) {
        const auto notes = buildInferInputNotes(piece.notes);
        InferParamCurve pitch;
        for (const auto &value : piece.inputPitch.values())
            pitch.values.append(value / 100.0);
        return {piece.clipId(), piece.id(), notes, configPath, appModel->tempo(), pitch};
    }

    QList<InferPiece *> findDirtyParamPieces(ParamInfo::Name name, SingingClip &clip) {
        QList<InferPiece *> result;
        for (auto &piece : clip.pieces()) {
            // 重新合并参数曲线，并与之前的缓存比较
            auto param = clip.params.getParamByName(name);
            auto original = *piece->getOriginalCurve(name);
            auto editedCurves = AppModelUtils::getDrawCurves(param->curves(Param::Edited));
            if (auto resultCurve = AppModelUtils::getResultCurve(original, editedCurves);
                resultCurve != original) {
                piece->setInputCurve(name, resultCurve);
                result.append(piece);
            }
        }
        return result;
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
        DrawCurve original;
        original.setStart(MathUtils::round(piece.realStartTick(), 5));
        original.setValues(
            Linq::selectMany(taskResult.values, L_PRED(v, static_cast<int>(v * scale))));
        piece.setOriginalCurve(name, original);
        // 合并手绘参数
        auto param = piece.clip->params.getParamByName(name);
        auto editedCurves = AppModelUtils::getDrawCurves(param->curves(Param::Edited));
        auto mergedCurve = AppModelUtils::getResultCurve(original, editedCurves);
        piece.setInputCurve(name, mergedCurve);
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
        piece.audioPath = taskResult;
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
        piece.setOriginalCurve(name, emptyCurve);
        piece.setInputCurve(name, emptyCurve);
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
        piece.acousticInferStatus = Pending;
    }
}