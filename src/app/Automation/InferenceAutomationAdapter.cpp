#include "InferenceAutomationAdapter.h"

#include "Modules/Inference/InferControllerHelper.h"
#include "Modules/Inference/Models/PhonemeNameResult.h"
#include "Modules/Inference/Models/PronunciationFetchResult.h"

#include <QSet>

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/InferenceData/InferPiece.h>
#include <lite/ProjectModel/InferenceData/InferSpeakerMix.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace Automation {
    namespace {
        ObjectRef clipRef(const ClipId id) {
            return {ObjectKind::Clip, id.value()};
        }

        ObjectRef pieceRef(const PieceId id) {
            return {ObjectKind::InferPiece, id.value()};
        }

        ObjectRef noteRef(const NoteId id) {
            return {ObjectKind::Note, id.value()};
        }

        AutomationResult<SingingClip *> resolveClip(AppModel *model, const ClipId id) {
            if (!model)
                return AutomationError::invalidArgument(QStringLiteral("model"),
                                                        QStringLiteral("AppModel is unavailable"));
            if (!id.isValid())
                return AutomationError::invalidArgument(QStringLiteral("clip_id"),
                                                        QStringLiteral("Clip ID is invalid"));
            auto *clipObject = model->findClipById(id.value());
            if (!clipObject)
                return AutomationError::notFound(clipRef(id), QStringLiteral("Clip was not found"));
            auto *clip = dynamic_cast<SingingClip *>(clipObject);
            if (!clip)
                return AutomationError::wrongObjectType(
                    clipRef(id), QStringLiteral("Clip is not a singing clip"));
            return clip;
        }

        AutomationResult<InferPiece *> resolvePiece(SingingClip *clip, const PieceId id) {
            if (!id.isValid())
                return AutomationError::invalidArgument(QStringLiteral("piece_id"),
                                                        QStringLiteral("Piece ID is invalid"));
            auto *piece = clip->findPieceById(id.value());
            if (!piece)
                return AutomationError::notFound(pieceRef(id),
                                                 QStringLiteral("Inference piece was not found"));
            return piece;
        }

        AutomationResult<QList<Note *>> resolveNotes(SingingClip *clip, const QList<NoteId> &ids) {
            QList<Note *> notes;
            notes.reserve(ids.count());
            for (const auto id : ids) {
                if (!id.isValid()) {
                    return AutomationError::invalidArgument(QStringLiteral("note_ids"),
                                                            QStringLiteral("Note ID is invalid"));
                }
                auto *note = clip->findNoteById(id.value());
                if (!note)
                    return AutomationError::notFound(noteRef(id),
                                                     QStringLiteral("Note was not found"));
                if (note->clip() != clip) {
                    return AutomationError::wrongObjectType(
                        noteRef(id), QStringLiteral("Note does not belong to the requested clip"));
                }
                if (notes.contains(note)) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("note_ids"), QStringLiteral("Note IDs contain duplicates"));
                }
                notes.append(note);
            }
            return notes;
        }

        QList<ObjectRef> affected(const ClipId clipId, const PieceId pieceId,
                                  const QList<NoteId> &noteIds = {}) {
            QList<ObjectRef> result{clipRef(clipId)};
            if (pieceId.isValid())
                result.append(pieceRef(pieceId));
            for (const auto id : noteIds)
                result.append(noteRef(id));
            return result;
        }

        bool phonemesEmpty(const Phonemes &phonemes) {
            return phonemes.nameSeq.original.isEmpty() && phonemes.nameSeq.edited.isEmpty() &&
                   phonemes.offsetSeq.original.isEmpty() && phonemes.offsetSeq.edited.isEmpty();
        }

        bool paramNeedsReset(const InferPiece &piece, const ParamInfo::Name name) {
            return !piece.getOriginalCurve(name)->isEmpty() ||
                   !piece.getInputCurve(name)->isEmpty();
        }

        bool pitchCascadeNeedsReset(const InferPiece &piece) {
            return paramNeedsReset(piece, ParamInfo::Pitch) ||
                   paramNeedsReset(piece, ParamInfo::Breathiness) ||
                   paramNeedsReset(piece, ParamInfo::Tension) ||
                   paramNeedsReset(piece, ParamInfo::Voicing) ||
                   paramNeedsReset(piece, ParamInfo::Energy) ||
                   paramNeedsReset(piece, ParamInfo::MouthOpening) || !piece.audioPath.isEmpty();
        }

        bool varianceCascadeNeedsReset(const InferPiece &piece) {
            return paramNeedsReset(piece, ParamInfo::Breathiness) ||
                   paramNeedsReset(piece, ParamInfo::Tension) ||
                   paramNeedsReset(piece, ParamInfo::Voicing) ||
                   paramNeedsReset(piece, ParamInfo::Energy) ||
                   paramNeedsReset(piece, ParamInfo::MouthOpening) || !piece.audioPath.isEmpty();
        }

        bool durationCascadeNeedsReset(const InferPiece &piece) {
            for (const auto *note : piece.notes) {
                if (!note->phonemeOffsetSeq().original.isEmpty())
                    return true;
                if ((note->isSlur() || note->isSyllabification()) &&
                    !note->phonemeOffsetSeq().edited.isEmpty()) {
                    return true;
                }
            }
            return pitchCascadeNeedsReset(piece);
        }

        bool stageNeedsReset(const InferPiece &piece, const InferenceStage stage) {
            switch (stage) {
                case InferenceStage::Duration:
                    return durationCascadeNeedsReset(piece);
                case InferenceStage::Pitch:
                    return pitchCascadeNeedsReset(piece);
                case InferenceStage::Variance:
                    return varianceCascadeNeedsReset(piece);
                case InferenceStage::Acoustic:
                    return !piece.audioPath.isEmpty();
            }
            return false;
        }

        bool varianceResetChangesDocument(const InferPiece &piece) {
            return paramNeedsReset(piece, ParamInfo::Breathiness) ||
                   paramNeedsReset(piece, ParamInfo::Tension) ||
                   paramNeedsReset(piece, ParamInfo::Voicing) ||
                   paramNeedsReset(piece, ParamInfo::Energy) ||
                   paramNeedsReset(piece, ParamInfo::MouthOpening);
        }

        bool pitchResetChangesDocument(const InferPiece &piece) {
            return paramNeedsReset(piece, ParamInfo::Pitch) || varianceResetChangesDocument(piece);
        }

        bool stageResetChangesDocument(const InferPiece &piece, const InferenceStage stage) {
            switch (stage) {
                case InferenceStage::Duration:
                    for (const auto *note : piece.notes) {
                        if (!note->phonemeOffsetSeq().original.isEmpty() ||
                            ((note->isSlur() || note->isSyllabification()) &&
                             !note->phonemeOffsetSeq().edited.isEmpty())) {
                            return true;
                        }
                    }
                    return pitchResetChangesDocument(piece);
                case InferenceStage::Pitch:
                    return pitchResetChangesDocument(piece);
                case InferenceStage::Variance:
                    return varianceResetChangesDocument(piece);
                case InferenceStage::Acoustic:
                    return false;
            }
            return false;
        }

        bool paramUpdateChanges(const InferPiece &piece, const ParamInfo::Name name,
                                const InferControllerHelper::ParamUpdate &update) {
            return *piece.getOriginalCurve(name) != update.original ||
                   *piece.getInputCurve(name) != update.input;
        }

        AutomationResult<AutomationUnit> validateCurve(const InferParamCurve &curve,
                                                       const QString &fieldPath) {
            for (const auto value : curve.values) {
                if (!std::isfinite(value)) {
                    return AutomationError::invalidArgument(
                        fieldPath, QStringLiteral("Inference curve contains a non-finite value"));
                }
            }
            return AutomationUnit{};
        }

        bool originalParamMatchesPieces(const SingingClip &clip, const ParamInfo::Name name) {
            QList<const DrawCurve *> expected;
            for (const auto *piece : clip.pieces()) {
                const auto *curve = piece->getOriginalCurve(name);
                if (!curve->isEmpty())
                    expected.append(curve);
            }
            const auto actual = clip.params.getParamByName(name)->curves(Param::Original);
            if (actual.count() != expected.count())
                return false;
            for (qsizetype i = 0; i < actual.count(); ++i) {
                const auto *curve = dynamic_cast<const DrawCurve *>(actual.at(i));
                if (!curve || *curve != *expected.at(i))
                    return false;
            }
            return true;
        }

        bool allOriginalParamsMatchPieces(const SingingClip &clip) {
            static constexpr ParamInfo::Name names[]{
                ParamInfo::Pitch,   ParamInfo::Breathiness, ParamInfo::Tension,
                ParamInfo::Voicing, ParamInfo::Energy,      ParamInfo::MouthOpening,
            };
            for (const auto name : names) {
                if (!originalParamMatchesPieces(clip, name))
                    return false;
            }
            return true;
        }

        AutomationResult<PreparedInferenceMutation>
            preparePronunciations(AppModel *model, const InferenceMutationRequest &request) {
            auto clipResult = resolveClip(model, request.clipId);
            if (!clipResult)
                return clipResult.getError();
            if (request.pronunciations.isEmpty()) {
                return AutomationError::invalidArgument(
                    QStringLiteral("pronunciations"),
                    QStringLiteral("Pronunciation result is empty"));
            }
            QList<NoteId> noteIds;
            QList<PronunciationFetchResult> values;
            for (const auto &entry : request.pronunciations) {
                noteIds.append(entry.noteId);
                values.append({entry.pronunciation, entry.candidates});
            }
            auto notesResult = resolveNotes(clipResult.get(), noteIds);
            if (!notesResult)
                return notesResult.getError();
            const auto notes = notesResult.get();
            bool persistedChanged = false;
            bool changed = false;
            for (qsizetype i = 0; i < notes.count(); ++i) {
                const bool pronunciationChanged =
                    notes.at(i)->pronunciation().original != values.at(i).pronunciation;
                persistedChanged = persistedChanged || pronunciationChanged;
                changed = changed || pronunciationChanged ||
                          notes.at(i)->pronCandidates() != values.at(i).candidates;
            }
            auto *clip = clipResult.get();
            return PreparedInferenceMutation{
                .changed = changed,
                .advancesRevision = persistedChanged,
                .affectedObjects = affected(request.clipId, {}, noteIds),
                .apply =
                    [clip, notes, values](InferenceMutationSideEffects &) {
                        InferControllerHelper::updatePronunciation(notes, values, *clip);
                    },
            };
        }

        AutomationResult<PreparedInferenceMutation>
            preparePhonemeNames(AppModel *model, const InferenceMutationRequest &request) {
            auto clipResult = resolveClip(model, request.clipId);
            if (!clipResult)
                return clipResult.getError();
            if (request.phonemeNames.isEmpty()) {
                return AutomationError::invalidArgument(QStringLiteral("phoneme_names"),
                                                        QStringLiteral("Phoneme result is empty"));
            }
            QList<NoteId> noteIds;
            QList<PhonemeNameResult> values;
            for (const auto &entry : request.phonemeNames) {
                noteIds.append(entry.noteId);
                values.append({true, entry.phonemeNames});
            }
            auto notesResult = resolveNotes(clipResult.get(), noteIds);
            if (!notesResult)
                return notesResult.getError();
            const auto notes = notesResult.get();
            bool persistedChanged = false;
            for (qsizetype i = 0; i < notes.count(); ++i) {
                const auto *note = notes.at(i);
                if (note->isSlur() || note->isSyllabification()) {
                    persistedChanged = persistedChanged || !phonemesEmpty(note->phonemes());
                    continue;
                }
                const auto &names = values.at(i).phonemeNames;
                persistedChanged = persistedChanged || note->phonemeNameSeq().original != names;
                if (note->phonemeNameSeq().result().count() != names.count())
                    persistedChanged =
                        persistedChanged || !note->phonemeOffsetSeq().original.isEmpty();
            }
            const bool changed = persistedChanged || !clipResult.get()->singerInfo().isEmpty();
            auto *clip = clipResult.get();
            return PreparedInferenceMutation{
                .changed = changed,
                .advancesRevision = persistedChanged,
                .affectedObjects = affected(request.clipId, {}, noteIds),
                .apply =
                    [model, clip, notes, values](InferenceMutationSideEffects &effects) {
                        InferControllerHelper::updatePhoneName(notes, values, *clip);
                        if (clip->singerInfo().isEmpty())
                            return;
                        const auto segmentation = clip->reSegment(model->timeline());
                        for (const auto *piece : segmentation.addedPieces)
                            effects.addedPieces.append(PieceId(piece->id()));
                        for (const auto id : segmentation.removedPieceIds)
                            effects.removedPieces.append(PieceId(id));
                    },
            };
        }

        AutomationResult<PreparedInferenceMutation>
            prepareDuration(AppModel *model, const InferenceMutationRequest &request) {
            auto clipResult = resolveClip(model, request.clipId);
            if (!clipResult)
                return clipResult.getError();
            auto pieceResult = resolvePiece(clipResult.get(), request.pieceId);
            if (!pieceResult)
                return pieceResult.getError();
            if (request.noteIds.count() != request.durationResult.count() ||
                request.noteIds.isEmpty()) {
                return AutomationError::invalidArgument(
                    QStringLiteral("duration_result"),
                    QStringLiteral("Duration result must match the non-empty note ID list"));
            }
            auto notesResult = resolveNotes(clipResult.get(), request.noteIds);
            if (!notesResult)
                return notesResult.getError();
            const auto notes = notesResult.get();
            const auto *piece = pieceResult.get();
            if (piece->notes.count() != notes.count()) {
                return AutomationError::invalidArgument(
                    QStringLiteral("note_ids"),
                    QStringLiteral("Duration notes do not match the requested inference piece"));
            }
            for (qsizetype i = 0; i < notes.count(); ++i) {
                if (piece->notes.at(i) != notes.at(i)) {
                    return AutomationError::wrongObjectType(
                        noteRef(request.noteIds.at(i)),
                        QStringLiteral("Note does not belong to the requested inference piece"));
                }
            }
            for (qsizetype i = 0; i < request.durationResult.count(); ++i) {
                const auto &result = request.durationResult.at(i);
                if (result.id != request.noteIds.at(i).value() ||
                    !std::is_sorted(result.phonemeOffsets.cbegin(), result.phonemeOffsets.cend())) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("duration_result"),
                        QStringLiteral("Duration result note mapping or offsets are invalid"));
                }
            }
            const auto offsets = InferControllerHelper::collectPhoneOffsetsForStorage(
                notes, request.durationResult, *clipResult.get(), model->timeline());
            if (offsets.count() != notes.count()) {
                return AutomationError::invalidArgument(
                    QStringLiteral("duration_result"),
                    QStringLiteral("Duration result cannot be mapped to the requested notes"));
            }
            bool changed = false;
            for (qsizetype i = 0; i < notes.count(); ++i) {
                const auto *note = notes.at(i);
                if (note->isSlur() || note->isSyllabification()) {
                    changed = changed || !note->phonemeOffsetSeq().original.isEmpty() ||
                              !note->phonemeOffsetSeq().edited.isEmpty();
                    continue;
                }
                const auto desired =
                    offsets.at(i).count() == note->phonemeNameSeq().result().count() ? offsets.at(i)
                                                                                     : QList<int>{};
                changed = changed || note->phonemeOffsetSeq().original != desired;
            }
            auto *clip = clipResult.get();
            return PreparedInferenceMutation{
                .changed = changed,
                .advancesRevision = changed,
                .affectedObjects = affected(request.clipId, request.pieceId, request.noteIds),
                .apply =
                    [model, clip, notes,
                     result = request.durationResult](InferenceMutationSideEffects &) {
                        InferControllerHelper::updatePhoneOffset(notes, result, *clip,
                                                                 model->timeline());
                        InferControllerHelper::getParamDirtyPiecesAndUpdateInput(
                            ParamInfo::Expressiveness, *clip, model->timeline());
                        InferControllerHelper::getParamDirtyPiecesAndUpdateInput(
                            ParamInfo::Gender, *clip, model->timeline());
                        InferControllerHelper::getParamDirtyPiecesAndUpdateInput(
                            ParamInfo::Velocity, *clip, model->timeline());
                    },
            };
        }

        AutomationResult<PreparedInferenceMutation>
            preparePitch(AppModel *model, const InferenceMutationRequest &request) {
            auto clipResult = resolveClip(model, request.clipId);
            if (!clipResult)
                return clipResult.getError();
            auto pieceResult = resolvePiece(clipResult.get(), request.pieceId);
            if (!pieceResult)
                return pieceResult.getError();
            if (request.pitchSmoothKernelSize < 0) {
                return AutomationError::invalidArgument(
                    QStringLiteral("pitch_smooth_kernel_size"),
                    QStringLiteral("Pitch smoothing kernel size is invalid"));
            }
            const auto curveValidation =
                validateCurve(request.pitchResult, QStringLiteral("pitch_result"));
            if (!curveValidation)
                return curveValidation.getError();
            auto *piece = pieceResult.get();
            const auto update = InferControllerHelper::buildParamUpdate(
                ParamInfo::Pitch, request.pitchResult, *piece, 100, request.pitchSmoothKernelSize);
            const bool changed = paramUpdateChanges(*piece, ParamInfo::Pitch, update);
            return PreparedInferenceMutation{
                .changed = changed,
                .advancesRevision = changed,
                .affectedObjects = affected(request.clipId, request.pieceId),
                .apply =
                    [piece, result = request.pitchResult,
                     smooth = request.pitchSmoothKernelSize](InferenceMutationSideEffects &) {
                        InferControllerHelper::updatePitch(result, *piece, smooth);
                    },
            };
        }

        AutomationResult<PreparedInferenceMutation>
            prepareVariance(AppModel *model, const InferenceMutationRequest &request) {
            auto clipResult = resolveClip(model, request.clipId);
            if (!clipResult)
                return clipResult.getError();
            auto pieceResult = resolvePiece(clipResult.get(), request.pieceId);
            if (!pieceResult)
                return pieceResult.getError();
            auto *piece = pieceResult.get();
            const auto &value = request.varianceResult;
            const QList<std::pair<const InferParamCurve *, QString>> curves{
                {&value.breathiness,  QStringLiteral("variance_result.breathiness")  },
                {&value.tension,      QStringLiteral("variance_result.tension")      },
                {&value.voicing,      QStringLiteral("variance_result.voicing")      },
                {&value.energy,       QStringLiteral("variance_result.energy")       },
                {&value.mouthOpening, QStringLiteral("variance_result.mouth_opening")},
            };
            for (const auto &[curve, fieldPath] : curves) {
                const auto validation = validateCurve(*curve, fieldPath);
                if (!validation)
                    return validation.getError();
            }
            const auto breathiness = InferControllerHelper::buildParamUpdate(
                ParamInfo::Breathiness, value.breathiness, *piece);
            const auto tension =
                InferControllerHelper::buildParamUpdate(ParamInfo::Tension, value.tension, *piece);
            const auto voicing =
                InferControllerHelper::buildParamUpdate(ParamInfo::Voicing, value.voicing, *piece);
            const auto energy =
                InferControllerHelper::buildParamUpdate(ParamInfo::Energy, value.energy, *piece);
            const auto mouthOpening = InferControllerHelper::buildParamUpdate(
                ParamInfo::MouthOpening, value.mouthOpening, *piece);
            const bool changed = paramUpdateChanges(*piece, ParamInfo::Breathiness, breathiness) ||
                                 paramUpdateChanges(*piece, ParamInfo::Tension, tension) ||
                                 paramUpdateChanges(*piece, ParamInfo::Voicing, voicing) ||
                                 paramUpdateChanges(*piece, ParamInfo::Energy, energy) ||
                                 paramUpdateChanges(*piece, ParamInfo::MouthOpening, mouthOpening);
            InferVarianceTask::InferVarianceResult result;
            result.breathiness = value.breathiness;
            result.tension = value.tension;
            result.voicing = value.voicing;
            result.energy = value.energy;
            result.mouthOpening = value.mouthOpening;
            return PreparedInferenceMutation{
                .changed = changed,
                .advancesRevision = changed,
                .affectedObjects = affected(request.clipId, request.pieceId),
                .apply =
                    [piece, result](InferenceMutationSideEffects &) {
                        InferControllerHelper::updateVariance(result, *piece);
                    },
            };
        }

        AutomationResult<PreparedInferenceMutation>
            prepareAcoustic(AppModel *model, const InferenceMutationRequest &request) {
            auto clipResult = resolveClip(model, request.clipId);
            if (!clipResult)
                return clipResult.getError();
            auto pieceResult = resolvePiece(clipResult.get(), request.pieceId);
            if (!pieceResult)
                return pieceResult.getError();
            if (request.acousticPath.isEmpty()) {
                return AutomationError::invalidArgument(QStringLiteral("acoustic_path"),
                                                        QStringLiteral("Acoustic path is empty"));
            }
            auto *piece = pieceResult.get();
            const bool changed =
                piece->audioPath != request.acousticPath || piece->acousticInferStatus != Success;
            return PreparedInferenceMutation{
                .changed = changed,
                .affectedObjects = affected(request.clipId, request.pieceId),
                .apply =
                    [piece, path = request.acousticPath](InferenceMutationSideEffects &) {
                        InferControllerHelper::updateAcoustic(path, *piece);
                    },
            };
        }

        AutomationResult<PreparedInferenceMutation>
            prepareResetStage(AppModel *model, const InferenceMutationRequest &request) {
            auto targets = request.pieceTargets;
            if (targets.isEmpty())
                targets.append({request.clipId, request.pieceId});

            QList<InferPiece *> pieces;
            QList<ObjectRef> affectedObjects;
            QSet<int> affectedClipIds;
            pieces.reserve(targets.size());
            affectedObjects.reserve(targets.size() * 2);
            affectedClipIds.reserve(targets.size());
            bool changed = false;
            bool advancesRevision = false;
            for (const auto &target : std::as_const(targets)) {
                auto clipResult = resolveClip(model, target.clipId);
                if (!clipResult)
                    return clipResult.getError();
                auto pieceResult = resolvePiece(clipResult.get(), target.pieceId);
                if (!pieceResult)
                    return pieceResult.getError();
                auto *piece = pieceResult.get();
                pieces.append(piece);
                if (!affectedClipIds.contains(target.clipId.value())) {
                    affectedClipIds.insert(target.clipId.value());
                    affectedObjects.append(clipRef(target.clipId));
                }
                affectedObjects.append(pieceRef(target.pieceId));
                const bool pieceChanged = stageNeedsReset(*piece, request.stage);
                changed |= pieceChanged;
                advancesRevision |=
                    pieceChanged && stageResetChangesDocument(*piece, request.stage);
            }

            return PreparedInferenceMutation{
                .changed = changed,
                .advancesRevision = advancesRevision,
                .affectedObjects = affectedObjects,
                .apply =
                    [pieces, stage = request.stage](InferenceMutationSideEffects &) {
                        for (auto *piece : pieces) {
                            switch (stage) {
                                case InferenceStage::Duration:
                                    InferControllerHelper::resetPhoneOffset(piece->notes, *piece);
                                    break;
                                case InferenceStage::Pitch:
                                    InferControllerHelper::resetPitch(*piece);
                                    break;
                                case InferenceStage::Variance:
                                    InferControllerHelper::resetVariance(*piece);
                                    break;
                                case InferenceStage::Acoustic:
                                    InferControllerHelper::resetAcoustic(*piece);
                                    break;
                            }
                        }
                    },
            };
        }

        AutomationResult<PreparedInferenceMutation>
            prepareInvalidateClip(AppModel *model, const InferenceMutationRequest &request) {
            auto clipResult = resolveClip(model, request.clipId);
            if (!clipResult)
                return clipResult.getError();
            auto *clip = clipResult.get();
            QList<PieceId> removed;
            for (const auto *piece : clip->pieces())
                removed.append(PieceId(piece->id()));
            return PreparedInferenceMutation{
                .changed = !removed.isEmpty(),
                .affectedObjects = affected(request.clipId, {}),
                .apply =
                    [clip, removed](InferenceMutationSideEffects &effects) {
                        clip->removeAllPieces();
                        effects.removedPieces = removed;
                    },
            };
        }

        AutomationResult<PreparedInferenceMutation>
            prepareResegmentClip(AppModel *model, const InferenceMutationRequest &request) {
            auto clipResult = resolveClip(model, request.clipId);
            if (!clipResult)
                return clipResult.getError();
            auto *clip = clipResult.get();
            const bool changed = clip->notes().count() > 0 || !clip->pieces().isEmpty();
            return PreparedInferenceMutation{
                .changed = changed,
                .affectedObjects = affected(request.clipId, {}),
                .apply =
                    [model, clip, bump = request.bumpClipInferenceRevision](
                        InferenceMutationSideEffects &effects) {
                        const auto segmentation = clip->reSegment(model->timeline(), bump);
                        for (const auto *piece : segmentation.addedPieces)
                            effects.addedPieces.append(PieceId(piece->id()));
                        for (const auto id : segmentation.removedPieceIds)
                            effects.removedPieces.append(PieceId(id));
                    },
            };
        }

        AutomationResult<PreparedInferenceMutation>
            prepareRefreshSpeakerMix(AppModel *model, const InferenceMutationRequest &request) {
            auto clipResult = resolveClip(model, request.clipId);
            if (!clipResult)
                return clipResult.getError();
            auto *clip = clipResult.get();
            QList<int> requestedPieceIds;
            for (const auto id : request.pieceIds) {
                auto resolved = resolvePiece(clip, id);
                if (!resolved)
                    return resolved.getError();
                if (requestedPieceIds.contains(id.value())) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("piece_ids"),
                        QStringLiteral("Piece IDs contain duplicates"));
                }
                requestedPieceIds.append(id.value());
            }

            struct Refresh {
                InferPiece *piece;
                InferSpeakerMix mix;
            };

            QList<Refresh> refreshes;
            bool changed = false;
            bool advancesRevision = false;
            const auto &timeline = model->timeline();
            for (auto *piece : clip->pieces()) {
                if (!requestedPieceIds.isEmpty() && !requestedPieceIds.contains(piece->id())) {
                    continue;
                }
                const auto mix = InferSpeakerMixModel::effectiveSpeakerMixFromData(
                    clip->speakerMixData(), clip->speakerId(),
                    clip->start() + piece->localStartTick(timeline),
                    clip->start() + piece->localEndTick(timeline), clip->start(), timeline);
                changed = changed || piece->speakerMix != mix ||
                          piece->speaker != mix.fallbackSpeaker || pitchCascadeNeedsReset(*piece) ||
                          piece->acousticInferStatus != Pending;
                advancesRevision = advancesRevision || pitchResetChangesDocument(*piece);
                refreshes.append({piece, mix});
            }
            auto affectedObjects = affected(request.clipId, {});
            for (const auto id : request.pieceIds)
                affectedObjects.append(pieceRef(id));
            return PreparedInferenceMutation{
                .changed = changed,
                .advancesRevision = changed && advancesRevision,
                .affectedObjects = std::move(affectedObjects),
                .apply =
                    [refreshes](InferenceMutationSideEffects &) {
                        for (const auto &refresh : refreshes) {
                            refresh.piece->speakerMix = refresh.mix;
                            refresh.piece->speaker = refresh.mix.fallbackSpeaker;
                            InferControllerHelper::resetPitch(*refresh.piece);
                            refresh.piece->acousticInferStatus = Pending;
                        }
                    },
            };
        }

        AutomationResult<PreparedInferenceMutation>
            prepareRefreshParamInput(AppModel *model, const InferenceMutationRequest &request) {
            auto clipResult = resolveClip(model, request.clipId);
            if (!clipResult)
                return clipResult.getError();
            if (request.parameterName == ParamInfo::Unknown ||
                request.parameterName == ParamInfo::SpeakerMix) {
                return AutomationError::invalidArgument(
                    QStringLiteral("parameter_name"),
                    QStringLiteral("Inference parameter name is invalid"));
            }
            auto *clip = clipResult.get();
            const auto updates = InferControllerHelper::buildParamInputUpdates(
                request.parameterName, *clip, model->timeline());
            QList<ObjectRef> affectedObjects = affected(request.clipId, {});
            for (const auto &update : updates)
                affectedObjects.append({ObjectKind::InferPiece, update.piece->id()});
            return PreparedInferenceMutation{
                .changed = !updates.isEmpty(),
                .affectedObjects = std::move(affectedObjects),
                .apply =
                    [updates, name = request.parameterName](InferenceMutationSideEffects &effects) {
                        for (const auto &update : updates) {
                            update.piece->setInputCurve(name, update.input);
                            effects.changedPieces.append(PieceId(update.piece->id()));
                        }
                    },
            };
        }

        AutomationResult<PreparedInferenceMutation>
            prepareRebuildOriginalParams(AppModel *model, const InferenceMutationRequest &request) {
            auto clipResult = resolveClip(model, request.clipId);
            if (!clipResult)
                return clipResult.getError();
            auto *clip = clipResult.get();
            const bool changed = !allOriginalParamsMatchPieces(*clip);
            return PreparedInferenceMutation{
                .changed = changed,
                .advancesRevision = changed,
                .affectedObjects = affected(request.clipId, {}),
                .apply =
                    [clip](InferenceMutationSideEffects &) {
                        InferControllerHelper::updateAllOriginalParam(*clip);
                    },
            };
        }

        AutomationResult<PreparedInferenceMutation>
            prepareMutation(AppModel *model, const InferenceMutationRequest &request) {
            switch (request.kind) {
                case InferenceMutationKind::ApplyPronunciations:
                    return preparePronunciations(model, request);
                case InferenceMutationKind::ApplyPhonemeNames:
                    return preparePhonemeNames(model, request);
                case InferenceMutationKind::ApplyDuration:
                    return prepareDuration(model, request);
                case InferenceMutationKind::ApplyPitch:
                    return preparePitch(model, request);
                case InferenceMutationKind::ApplyVariance:
                    return prepareVariance(model, request);
                case InferenceMutationKind::ApplyAcoustic:
                    return prepareAcoustic(model, request);
                case InferenceMutationKind::ResetStage:
                    return prepareResetStage(model, request);
                case InferenceMutationKind::InvalidateClip:
                    return prepareInvalidateClip(model, request);
                case InferenceMutationKind::ResegmentClip:
                    return prepareResegmentClip(model, request);
                case InferenceMutationKind::RefreshSpeakerMix:
                    return prepareRefreshSpeakerMix(model, request);
                case InferenceMutationKind::RefreshParamInput:
                    return prepareRefreshParamInput(model, request);
                case InferenceMutationKind::RebuildOriginalParams:
                    return prepareRebuildOriginalParams(model, request);
            }
            return AutomationError::invalidArgument(
                QStringLiteral("kind"), QStringLiteral("Inference mutation is invalid"));
        }
    }

    InferenceRuntimeServices createInferenceAutomationServices() {
        InferenceRuntimeServices services;
        services.prepareMutation = prepareMutation;
        return services;
    }

} // namespace Automation
