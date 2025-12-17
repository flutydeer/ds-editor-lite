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
#include "Utils/ParamUtils.h"
#include "curve-util/CurveUtil.h"

#include <QDebug>

#include "Model/AppOptions/AppOptions.h"

namespace InferControllerHelper {
    QList<InferInputNote> buildInferInputNotes(const QList<Note *> &notes) {
        QList<InferInputNote> list;
        for (const auto note : notes)
            list.append(InferInputNote(*note));
        return list;
    }

    DurInput buildInferDurInput(const InferPiece &piece, const SingerIdentifier &identifier) {
        DurInput input;
        input.clipId = piece.clip->id();
        input.pieceId = piece.id();

        input.headAvailableLengthMs = piece.headAvailableLengthMs;
        input.paddingStartMs = piece.paddingStartMs;
        input.paddingEndMs = piece.paddingEndMs;

        input.timeline = {{{0, appModel->tempo()}}};
        input.notes = buildInferInputNotes(piece.notes);

        input.speaker = piece.speaker;
        input.identifier = identifier;
        input.steps = appOptions->inference()->samplingSteps;
        return input;
    }

    PitchInput buildInferPitchInput(const InferPiece &piece, const SingerIdentifier &identifier) {
        InferParamCurve expr;
        for (const auto &value : piece.inputExpressiveness.values())
            expr.values.append(value / 1000.0);

        PitchInput input;
        input.clipId = piece.clipId();
        input.pieceId = piece.id();

        input.headAvailableLengthMs = piece.headAvailableLengthMs;
        input.paddingStartMs = piece.paddingStartMs;
        input.paddingEndMs = piece.paddingEndMs;

        input.timeline = {{{0, appModel->tempo()}}};
        input.notes = buildInferInputNotes(piece.notes);
        input.expressiveness = expr;

        input.speaker = piece.speaker;
        input.identifier = identifier;
        input.steps = appOptions->inference()->samplingSteps;
        return input;
    }

    VarianceInput buildInferVarianceInput(const InferPiece &piece,
                                          const SingerIdentifier &identifier) {
        InferParamCurve pitch;
        for (const auto &value : piece.inputPitch.values())
            pitch.values.append(value / 100.0);

        VarianceInput input;
        input.clipId = piece.clipId();
        input.pieceId = piece.id();

        input.headAvailableLengthMs = piece.headAvailableLengthMs;
        input.paddingStartMs = piece.paddingStartMs;
        input.paddingEndMs = piece.paddingEndMs;

        input.timeline = {{{0, appModel->tempo()}}};
        input.notes = buildInferInputNotes(piece.notes);
        input.pitch = pitch;

        input.speaker = piece.speaker;
        input.identifier = identifier;
        input.steps = appOptions->inference()->samplingSteps;
        return input;
    }

    AcousticInput buildInderAcousticInput(const InferPiece &piece,
                                          const SingerIdentifier &identifier) {
        InferParamCurve pitch;
        for (const auto &value : piece.inputPitch.values())
            pitch.values.append(value / 100.0);

        InferParamCurve breathiness;
        for (const auto &value : piece.inputBreathiness.values())
            breathiness.values.append(value / 1000.0);

        InferParamCurve tension;
        for (const auto &value : piece.inputTension.values())
            tension.values.append(value / 1000.0);

        InferParamCurve voicing;
        for (const auto &value : piece.inputVoicing.values())
            voicing.values.append(value / 1000.0);

        InferParamCurve energy;
        for (const auto &value : piece.inputEnergy.values())
            energy.values.append(value / 1000.0);

        InferParamCurve mouthOpening;
        for (const auto &value : piece.inputMouthOpening.values())
            mouthOpening.values.append(value / 1000.0);

        InferParamCurve gender;
        for (const auto &value : piece.inputGender.values())
            gender.values.append(value / 1000.0);

        const InferParamCurve velocity = {
            Linq::selectMany(piece.inputVelocity.values(), L_PRED(p, p / 1000.0))};

        const InferParamCurve toneShift = {
            Linq::selectMany(piece.inputToneShift.values(), L_PRED(p, p * 1.0))};

        AcousticInput input;
        input.clipId = piece.clipId();
        input.pieceId = piece.id();

        input.headAvailableLengthMs = piece.headAvailableLengthMs;
        input.paddingStartMs = piece.paddingStartMs;
        input.paddingEndMs = piece.paddingEndMs;

        input.timeline = {{{0, appModel->tempo()}}};
        input.notes = buildInferInputNotes(piece.notes);
        input.pitch = pitch;
        input.breathiness = breathiness;
        input.tension = tension;
        input.voicing = voicing;
        input.energy = energy;
        input.mouthOpening = mouthOpening;
        input.gender = gender;
        input.velocity = velocity;
        input.toneShift = toneShift;

        input.speaker = piece.speaker;
        input.identifier = identifier;
        input.steps = appOptions->inference()->samplingSteps;
        return input;
    }

    QList<InferPiece *> getParamDirtyPiecesAndUpdateInput(const ParamInfo::Name name,
                                                          SingingClip &clip) {
        QList<InferPiece *> result;
        for (auto &piece : clip.pieces()) {
            // 重新合并参数曲线，并与之前的缓存比较
            const auto param = clip.params.getParamByName(name);
            auto editedCurves = AppModelUtils::getDrawCurves(param->curves(Param::Edited));
            auto input = *piece->getInputCurve(name);
            if (ParamInfo::hasOriginalParam(name)) {
                auto original = *piece->getOriginalCurve(name);
                if (auto resultCurve = AppModelUtils::getResultCurve(original, editedCurves);
                    resultCurve != input) {
                    piece->setInputCurve(name, resultCurve);
                    result.append(piece);
                }
            } else {
                const auto baseValue = paramUtils->getPropertiesByName(name)->defaultValue;
                if (auto resultCurve = AppModelUtils::getResultCurve(
                        {piece->localStartTick(), piece->localEndTick()}, baseValue, editedCurves);
                    resultCurve != input) {
                    piece->setInputCurve(name, resultCurve);
                    result.append(piece);
                }
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
        const auto &[alignTick, alignValues] = CurveUtil::alignCurve(
            piece.localStartTick(), 5, {taskResult.values.begin(), taskResult.values.end()}, 5);

        const auto smooth_kernel_size = appOptions->inference()->pitch_smooth_kernel_size;
        std::vector<double> paramValue;
        if (name == ParamInfo::Pitch && smooth_kernel_size > 0) {
            const auto smoother =
                std::make_unique<CurveUtil::SinusoidalSmoothingConv1d>(smooth_kernel_size);
            paramValue = smoother->forward(alignValues);
        } else
            paramValue = alignValues;

        // 将推理结果保存到分段内部
        DrawCurve original;
        original.setLocalStart(alignTick);
        original.setValues(Linq::selectMany(paramValue, L_PRED(v, static_cast<int>(v * scale))));
        piece.setOriginalCurve(name, original);
        // 合并手绘参数
        const auto param = piece.clip->params.getParamByName(name);
        const auto editedCurves = AppModelUtils::getDrawCurves(param->curves(Param::Edited));
        const auto mergedCurve = AppModelUtils::getResultCurve(original, editedCurves);
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
        updateParam(ParamInfo::MouthOpening, taskResult.mouthOpening, piece);
    }

    void updateAcoustic(const QString &taskResult, InferPiece &piece) {
        piece.audioPath = taskResult;
        piece.acousticInferStatus = Success;
    }

    void updateAllOriginalParam(SingingClip &clip) {
        clip.updateOriginalParam(ParamInfo::Pitch);
        clip.updateOriginalParam(ParamInfo::Breathiness);
        clip.updateOriginalParam(ParamInfo::Tension);
        clip.updateOriginalParam(ParamInfo::Voicing);
        clip.updateOriginalParam(ParamInfo::Energy);
        clip.updateOriginalParam(ParamInfo::MouthOpening);
    }

    void resetPhoneOffset(const QList<Note *> &notes, InferPiece &piece, const bool cascadeReset) {
        if (cascadeReset)
            resetPitch(piece);
        for (const auto note : notes) {
            note->setPhonemeOffsetInfo(Phonemes::Ahead, Note::Original, {});
            note->setPhonemeOffsetInfo(Phonemes::Normal, Note::Original, {});
        }
        piece.clip->notifyNoteChanged(SingingClip::OriginalWordPropertyChange, notes);
    }

    void resetParam(const ParamInfo::Name name, InferPiece &piece) {
        const DrawCurve emptyCurve;
        piece.setOriginalCurve(name, emptyCurve);
        piece.setInputCurve(name, emptyCurve);
        piece.clip->updateOriginalParam(name);
    }

    void resetPitch(InferPiece &piece, const bool cascadeReset) {
        if (cascadeReset)
            resetVariance(piece);
        resetParam(ParamInfo::Pitch, piece);
    }

    void resetVariance(InferPiece &piece, const bool cascadeReset) {
        if (cascadeReset)
            resetAcoustic(piece);
        resetParam(ParamInfo::Breathiness, piece);
        resetParam(ParamInfo::Tension, piece);
        resetParam(ParamInfo::Voicing, piece);
        resetParam(ParamInfo::Energy, piece);
        resetParam(ParamInfo::MouthOpening, piece);
    }

    void resetAcoustic(InferPiece &piece) {
        piece.audioPath = QString();
        piece.acousticInferStatus = Pending;
    }
}