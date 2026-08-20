#include "InferControllerHelper.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AnchorCurve.h>
#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/InferenceData/InferPiece.h>
#include "Models/InferInputNote.h"
#include "Tasks/Syllabification.h"
#include <lite/ProjectModel/InferenceData/InferSpeakerMix.h>
#include "Models/SpeakerMixValidator.h"
#include <lite/ProjectModel/Utils/AppModelUtils.h>
#include <lite/Support/Linq.h>
#include <lite/Support/MathUtils.h>
#include "Model/Utils/ParamUtils.h"
#include "curve-util/CurveUtil.h"

#include <QDebug>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(logInferHelper, "infer.helper")

#include "Model/AppOptions/AppOptions.h"

namespace InferControllerHelper {

    namespace {
        /// Resolve and validate the (speaker, speakerMix) pair for a piece
        /// against the clip's singer capability. Returns the sanitized pair
        /// plus a flag indicating whether a degradation warning was emitted.
        struct ValidatedSpeaker {
            QString speaker;
            InferSpeakerMix speakerMix;
        };

        ValidatedSpeaker resolveSpeakerForPiece(const InferPiece &piece) {
            const auto singerInfo = piece.clip->singerInfo();
            const auto rawMix = InferSpeakerMixModel::effectiveSpeakerMixForPiece(piece);
            const auto validated = SpeakerMixValidator::validate(piece.speaker, rawMix, singerInfo);
            if (!validated.ok()) {
                qWarning().noquote() << "[SpeakerMixValidator]" << validated.warningMessage;
            }
            return {validated.primarySpeaker, validated.sanitizedMix};
        }

        void populateBaseInput(InferInputBase &input, const InferPiece &piece,
                               const SingerIdentifier &identifier) {
            input.clipId = piece.clipId();
            input.pieceId = piece.id();
            input.clipRevision = piece.clip->inferenceRevision();
            input.clipStartTick = piece.clip->start();
            input.timeline = appModel->timeline();
            const auto headLayout = piece.phonemeHeadLayout();
            const auto firstNoteGlobalTick =
                input.clipStartTick + piece.notes.first()->localStart();
            input.pieceStartTick = headLayout.pieceStartTick(input.timeline, firstNoteGlobalTick);
            input.pieceEndTick = input.clipStartTick + piece.localEndTick(input.timeline);
            input.headAvailableLengthMs = piece.headAvailableLengthMs;
            input.paddingStartMs = piece.paddingStartMs;
            input.paddingEndMs = piece.paddingEndMs;
            input.minimumFirstOffsetMs = headLayout.minimumFirstOffsetMs;
            input.requiredHeadLengthMs = headLayout.requiredHeadLengthMs;
            input.maximumHeadLengthMs = headLayout.maximumHeadLengthMs;
            input.notes = buildInferInputNotes(piece.notes);
            QStringList lyrics;
            lyrics.reserve(piece.notes.size());
            for (const auto note : piece.notes)
                lyrics.append(note->lyric());
            Syllabification::distributeForInference(lyrics, input.notes, input.timeline,
                                                    input.clipStartTick);

            const auto spk = resolveSpeakerForPiece(piece);
            input.speaker = spk.speaker;
            input.speakerMix = spk.speakerMix;
            input.identifier = identifier;
            input.steps = appOptions->inference()->samplingSteps;
            input.pitchSmoothKernelSize = appOptions->inference()->pitch_smooth_kernel_size;
        }

        InferParamCurve curveSnapshot(const DrawCurve &curve, const double scale) {
            InferParamCurve result;
            result.localStartTick = curve.localStart();
            result.values.reserve(curve.values().size());
            for (const auto value : curve.values())
                result.values.append(value / scale);
            return result;
        }

        template <typename F>
        InferParamCurve curveTransformedSnapshot(const DrawCurve &curve, const double scale,
                                                 F unaryOp) {
            InferParamCurve result;
            result.localStartTick = curve.localStart();
            result.values.reserve(curve.values().size());
            for (const auto value : curve.values())
                result.values.append(unaryOp(value / scale));
            return result;
        }
    } // namespace

    DrawCurveList getEditedCurvesIncludingAnchor(const Param *param,
                                                 QList<DrawCurve *> &ownedCurves) {
        auto curves = AppModelUtils::getDrawCurves(param->curves(Param::Edited));
        for (auto *curve : param->curves(Param::Edited)) {
            if (curve->type() == Curve::Anchor) {
                auto *dc = dynamic_cast<AnchorCurve *>(curve)->toDrawCurve();
                if (dc) {
                    curves.append(dc);
                    ownedCurves.append(dc);
                }
            }
        }
        return curves;
    }

    QList<InferInputNote> buildInferInputNotes(const QList<Note *> &notes) {
        QList<InferInputNote> list;
        for (const auto note : notes)
            list.append(InferInputNote(*note));
        return list;
    }

    DurInput buildInferDurInput(const InferPiece &piece, const SingerIdentifier &identifier) {
        DurInput input;
        populateBaseInput(input, piece, identifier);
        return input;
    }

    PitchInput buildInferPitchInput(const InferPiece &piece, const SingerIdentifier &identifier) {
        PitchInput input;
        populateBaseInput(input, piece, identifier);
        input.expressiveness = curveSnapshot(piece.inputExpressiveness, 1000.0);
        return input;
    }

    VarianceInput buildInferVarianceInput(const InferPiece &piece,
                                          const SingerIdentifier &identifier) {
        VarianceInput input;
        populateBaseInput(input, piece, identifier);
        input.pitch = curveSnapshot(piece.inputPitch, 100.0);
        input.toneShift = curveSnapshot(piece.inputToneShift, 1.0);
        return input;
    }

    AcousticInput buildInferAcousticInput(const InferPiece &piece,
                                          const SingerIdentifier &identifier) {
        AcousticInput input;
        populateBaseInput(input, piece, identifier);
        input.pitch = curveSnapshot(piece.inputPitch, 100.0);
        input.breathiness = curveSnapshot(piece.inputBreathiness, 1000.0);
        input.tension = curveSnapshot(piece.inputTension, 1000.0);
        input.voicing = curveSnapshot(piece.inputVoicing, 1000.0);
        input.energy = curveSnapshot(piece.inputEnergy, 1000.0);
        input.mouthOpening = curveSnapshot(piece.inputMouthOpening, 1000.0);
        input.gender = curveSnapshot(piece.inputGender, 1000.0);
        input.velocity = curveTransformedSnapshot(piece.inputVelocity, 1000.0,
                                                  [](double x) { return std::exp2(x); });
        input.toneShift = curveSnapshot(piece.inputToneShift, 1.0);
        input.depth = appOptions->inference()->depth;
        return input;
    }

    QString buildSemanticSignature(const QString &taskType, const InferPiece &piece,
                                   const SingerIdentifier &identifier) {
        if (taskType == "duration")
            return buildInferDurInput(piece, identifier).semanticSignature();
        if (taskType == "pitch")
            return buildInferPitchInput(piece, identifier).semanticSignature();
        if (taskType == "variance")
            return buildInferVarianceInput(piece, identifier).semanticSignature();
        if (taskType == "acoustic")
            return buildInferAcousticInput(piece, identifier).semanticSignature();
        return {};
    }

    QList<InferPiece *> getParamDirtyPiecesAndUpdateInput(const ParamInfo::Name name,
                                                          SingingClip &clip) {
        QList<InferPiece *> result;
        for (auto &piece : clip.pieces()) {
            // 重新合并参数曲线，并与之前的缓存比较
            const auto param = clip.params.getParamByName(name);
            QList<DrawCurve *> ownedCurves;
            auto editedCurves = getEditedCurvesIncludingAnchor(param, ownedCurves);
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
                if (auto resultCurve =
                        AppModelUtils::getResultCurve({piece->localStartTick(appModel->timeline()),
                                                       piece->localEndTick(appModel->timeline())},
                                                      baseValue, editedCurves);
                    resultCurve != input) {
                    piece->setInputCurve(name, resultCurve);
                    result.append(piece);
                }
            }
            qDeleteAll(ownedCurves);
        }
        return result;
    }

    void updatePronunciation(const QList<Note *> &notes,
                             const QList<PronunciationFetchResult> &args, SingingClip &clip) {
        if (notes.count() != args.count()) {
            qCCritical(logInferHelper)
                << "updateNotesPronunciation() note count != args count:" << notes.count()
                << args.count() << "clipId:" << clip.id();
            return;
        }
        int i = 0;
        for (const auto note : notes) {
            note->setPronunciation(Note::Original, args[i].pronunciation);
            note->setPronCandidates(args[i].candidates);
            i++;
        }
        clip.notifyNoteChanged(SingingClip::OriginalWordPropertyChange, notes);
    }

    void updatePhoneName(const QList<Note *> &notes, const QList<PhonemeNameResult> &args,
                         SingingClip &clip) {
        if (notes.count() != args.count()) {
            qCCritical(logInferHelper)
                << "updatePhoneName() note count != args count:" << notes.count() << args.count()
                << "clipId:" << clip.id();
            return;
        }
        int i = 0;
        // Update regardless of whether phoneme names are successfully retrieved
        for (const auto note : notes) {
            if (note->isSlur() || Syllabification::isSyllabificationLyric(note->lyric())) {
                note->setPhonemes({});
                ++i;
                continue;
            }
            if (note->phonemeNameSeq().result().count() != args[i].phonemeNames.count())
                note->setPhonemeOffsetSeq(Note::Original, {});
            note->setPhonemeNameSeq(Note::Original, args[i].phonemeNames);
            ++i;
        }
        clip.notifyNoteChanged(SingingClip::OriginalWordPropertyChange, notes);
    }

    void updatePhoneOffset(const QList<Note *> &notes, const QList<InferInputNote> &args,
                           SingingClip &clip) {
        if (notes.count() != args.count()) {
            qCCritical(logInferHelper)
                << "updateNotesPhonemeName() note count != args count:" << notes.count()
                << args.count() << "clipId:" << clip.id();
            return;
        }
        QStringList lyrics;
        lyrics.reserve(notes.size());
        for (const auto note : notes)
            lyrics.append(note->lyric());
        const auto storedOffsets =
            Syllabification::collectForStorage(lyrics, args, appModel->timeline(), clip.start());

        for (int i = 0; i < notes.size(); ++i) {
            const auto note = notes.at(i);
            if (note->isSlur() || Syllabification::isSyllabificationLyric(note->lyric())) {
                note->setPhonemeOffsetSeq({});
                continue;
            }
            if (storedOffsets.at(i).size() != note->phonemeNameSeq().result().size()) {
                qCCritical(logInferHelper)
                    << "updatePhoneOffset() merged offset count does not match stored names"
                    << "noteId:" << note->id() << "offsetCount:" << storedOffsets.at(i).size()
                    << "nameCount:" << note->phonemeNameSeq().result().size();
                note->setPhonemeOffsetSeq(Note::Original, {});
                continue;
            }
            note->setPhonemeOffsetSeq(Note::Original, storedOffsets.at(i));
        }
        clip.notifyNoteChanged(SingingClip::OriginalWordPropertyChange, notes);
    }

    void updateParam(const ParamInfo::Name name, const InferParamCurve &taskResult,
                     InferPiece &piece, int scale, int smoothKernelSize) {
        const auto alignTick = taskResult.localStartTick;
        const std::vector<double> alignValues{taskResult.values.begin(), taskResult.values.end()};

        if (smoothKernelSize < 0)
            smoothKernelSize = appOptions->inference()->pitch_smooth_kernel_size;
        std::vector<double> paramValue;
        if (name == ParamInfo::Pitch && smoothKernelSize > 0) {
            const auto smoother =
                std::make_unique<CurveUtil::SinusoidalSmoothingConv1d>(smoothKernelSize);
            paramValue = smoother->forward(alignValues);
        } else
            paramValue = alignValues;

        // 将推理结果保存到分段内部
        DrawCurve original;
        original.setLocalStart(alignTick);
        original.setValues(Linq::selectMany(paramValue, L_PRED(v, static_cast<int>(v * scale))));
        piece.setOriginalCurve(name, original);
        const auto param = piece.clip->params.getParamByName(name);
        QList<DrawCurve *> ownedCurves;
        const auto editedCurves = getEditedCurvesIncludingAnchor(param, ownedCurves);
        const auto mergedCurve = AppModelUtils::getResultCurve(original, editedCurves);
        piece.setInputCurve(name, mergedCurve);
        qDeleteAll(ownedCurves);
        piece.clip->updateOriginalParam(name);
    }

    void updatePitch(const InferParamCurve &taskResult, InferPiece &piece,
                     const int smoothKernelSize) {
        updateParam(ParamInfo::Pitch, taskResult, piece, 100, smoothKernelSize);
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
            note->setPhonemeOffsetSeq(Note::Original, {});
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
    }
}
