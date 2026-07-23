//
// Created by FlutyDeer on 2026/6/7.
//

#include "InferenceApplyGate.h"

#include "Modules/Inference/EditSessionManager.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/InferPiece.h"
#include "Model/AppModel/Note.h"
#include "Model/AppModel/SingingClip.h"
#include "Modules/Inference/InferControllerHelper.h"
#include "Model/AppModel/InferSpeakerMix.h"

#include <QDebug>

namespace {
    QString decisionName(const InferenceApplyGate::Decision decision) {
        switch (decision) {
            case InferenceApplyGate::Decision::Apply:
                return "Apply";
            case InferenceApplyGate::Decision::Drop:
                return "Drop";
            case InferenceApplyGate::Decision::Defer:
                return "Defer";
        }
        return "Unknown";
    }

    bool intersects(const QList<int> &left, const QList<int> &right) {
        for (const auto value : left) {
            if (right.contains(value))
                return true;
        }
        return false;
    }

    bool sessionContainsClip(const EditSession &session, const int clipId) {
        return session.clipId == clipId || session.clipIds.contains(clipId);
    }

    QList<int> resolvedNoteIds(const InferenceTaskContext &context,
                               const InferenceTaskResolution &resolution) {
        auto noteIds = context.noteIds;
        for (const auto note : resolution.notes) {
            if (note && !noteIds.contains(note->id()))
                noteIds.append(note->id());
        }
        if (resolution.piece) {
            for (const auto note : resolution.piece->notes) {
                if (note && !noteIds.contains(note->id()))
                    noteIds.append(note->id());
            }
        }
        return noteIds;
    }

    bool noteOrPhonemeSessionConflicts(const EditSession &session,
                                       const InferenceTaskContext &context,
                                       const InferenceTaskResolution &resolution) {
        if (!sessionContainsClip(session, context.clipId))
            return false;
        if (session.wholeClipScope)
            return true;
        if (context.pieceId >= 0 && session.pieceIds.contains(context.pieceId))
            return true;
        const auto noteIds = resolvedNoteIds(context, resolution);
        if (!session.noteIds.isEmpty())
            return intersects(session.noteIds, noteIds);
        return noteIds.isEmpty();
    }

    bool paramConflictsWithTask(const ParamInfo::Name param, const QString &taskType) {
        const auto type = taskType.toLower();
        const bool isPitch = type == "pitch";
        const bool isVariance = type == "variance";
        const bool isAcoustic = type == "acoustic";

        switch (param) {
            case ParamInfo::Expressiveness:
                return isPitch || isVariance || isAcoustic;
            case ParamInfo::Pitch:
                return isPitch || isVariance || isAcoustic;
            case ParamInfo::Energy:
            case ParamInfo::Breathiness:
            case ParamInfo::Voicing:
            case ParamInfo::Tension:
            case ParamInfo::MouthOpening:
                return isVariance || isAcoustic;
            case ParamInfo::Gender:
            case ParamInfo::Velocity:
            case ParamInfo::ToneShift:
                return isAcoustic;
            case ParamInfo::SpeakerMix:
            case ParamInfo::Unknown:
                return false;
        }
        return false;
    }

    bool paramSessionConflicts(const EditSession &session, const InferenceTaskContext &context) {
        if (!sessionContainsClip(session, context.clipId))
            return false;
        if (session.params.isEmpty())
            return true;
        for (const auto param : session.params) {
            if (paramConflictsWithTask(param, context.taskType))
                return true;
        }
        return false;
    }

    bool editSessionConflicts(const EditSession &session, const InferenceTaskContext &context,
                              const InferenceTaskResolution &resolution) {
        switch (session.domain) {
            case AppStatus::EditObjectType::None:
                return false;
            case AppStatus::EditObjectType::Clip:
                return sessionContainsClip(session, context.clipId);
            case AppStatus::EditObjectType::Note:
            case AppStatus::EditObjectType::Phoneme:
                return noteOrPhonemeSessionConflicts(session, context, resolution);
            case AppStatus::EditObjectType::Param:
                return paramSessionConflicts(session, context);
        }
        return false;
    }

    bool shouldCheckSpeakerMixSignature(const InferenceTaskContext &context) {
        return !context.speakerMixSignature.isEmpty();
    }

    bool hasPieceInputSignature(const InferenceTaskContext &context,
                                const InferenceApplyGate::Options &options) {
        return options.requirePiece && !context.inputSignature.isEmpty();
    }
}

namespace InferenceApplyGate {
    void logDecision(const InferenceTaskContext &context, const QString &phase,
                     const Decision decision, const QString &reason,
                     const quint64 currentRevision) {
        if (editSessionManager->hasActiveTransaction()) {
            const auto session = editSessionManager->activeSession();
            qInfo() << "Inference apply decision" << "decision:" << decisionName(decision)
                    << "reason:" << reason << "phase:" << phase << "taskType:" << context.taskType
                    << "taskId:" << context.taskId << "clipId:" << context.clipId
                    << "pieceId:" << context.pieceId << "taskRevision:" << context.clipRevision
                    << "currentRevision:" << currentRevision << "sessionId:" << session.sessionId
                    << "sessionDomain:" << static_cast<int>(session.domain);
            return;
        }

        qInfo() << "Inference apply decision" << "decision:" << decisionName(decision)
                << "reason:" << reason << "phase:" << phase << "taskType:" << context.taskType
                << "taskId:" << context.taskId << "clipId:" << context.clipId
                << "pieceId:" << context.pieceId << "taskRevision:" << context.clipRevision
                << "currentRevision:" << currentRevision;
    }

    void logDrop(const InferenceTaskContext &context, const QString &phase, const QString &reason,
                 const quint64 currentRevision) {
        logDecision(context, phase, Decision::Drop, reason, currentRevision);
    }

    Decision resolve(const InferenceTaskContext &context, InferenceTaskResolution &resolution,
                     const Options &options) {
        const auto drop = [&resolution, &context, &options](const QString &reason,
                                                            const quint64 currentRevision = 0) {
            logDecision(context, options.phase, Decision::Drop, reason, currentRevision);
            resolution = {};
            resolution.dropReason = reason;
            return Decision::Drop;
        };

        const auto defer = [&resolution, &context, &options](const QString &reason,
                                                             const quint64 currentRevision = 0) {
            logDecision(context, options.phase, Decision::Defer, reason, currentRevision);
            resolution.dropReason = reason;
            return Decision::Defer;
        };

        if (context.clipId < 0)
            return drop("context-missing");

        const auto clipObject = appModel->findClipById(context.clipId);
        if (!clipObject)
            return drop("clip-not-found");

        const auto clip = dynamic_cast<SingingClip *>(clipObject);
        if (!clip)
            return drop("not-singing-clip");

        const auto currentRevision = clip->inferenceRevision();
        InferPiece *piece = nullptr;
        if (options.requirePiece) {
            if (context.pieceId < 0)
                return drop("piece-context-missing", currentRevision);
            piece = clip->findPieceById(context.pieceId);
            if (!piece)
                return drop("piece-not-found", currentRevision);
            if (piece->clip != clip || piece->clipId() != clip->id())
                return drop("piece-clip-mismatch", currentRevision);
            if (options.checkSingerSpeaker &&
                (piece->identifier != context.singer || piece->speaker != context.speaker)) {
                return drop("piece-singer-speaker-mismatch", currentRevision);
            }
            if (options.checkSingerSpeaker && shouldCheckSpeakerMixSignature(context) &&
                InferSpeakerMixModel::effectiveSpeakerMixForPiece(*piece).signature() != context.speakerMixSignature) {
                return drop("piece-speaker-mix-mismatch", currentRevision);
            }
        }

        if (options.checkSingerSpeaker) {
            const auto currentMix =
                piece ? InferSpeakerMixModel::effectiveSpeakerMixForPiece(*piece)
                      : InferSpeakerMixModel::effectiveSpeakerMixForFixedInference(
                            clip->speakerMixData(), clip->speakerId());
            if (clip->singerIdentifier() != context.singer ||
                currentMix.fallbackSpeaker != context.speaker) {
                return drop("clip-singer-speaker-mismatch", currentRevision);
            }
            if (shouldCheckSpeakerMixSignature(context) &&
                currentMix.signature() != context.speakerMixSignature)
                return drop("clip-speaker-mix-mismatch", currentRevision);
        }

        if (options.expectedNoteCount >= 0 && context.noteIds.count() != options.expectedNoteCount)
            return drop("result-count-mismatch", currentRevision);

        if (options.requireNotesInPiece && piece && piece->notes.count() != context.noteIds.count())
            return drop("piece-note-count-mismatch", currentRevision);

        QList<Note *> notes;
        notes.reserve(context.noteIds.count());
        for (const auto noteId : context.noteIds) {
            const auto note = clip->findNoteById(noteId);
            if (!note)
                return drop("note-not-found", currentRevision);
            if (note->clip() != clip)
                return drop("note-not-in-clip", currentRevision);
            if (options.requireNotesInPiece && piece && !piece->notes.contains(note))
                return drop("note-not-in-piece", currentRevision);
            notes.append(note);
        }

        resolution = {clip, piece, notes, {}};
        if (hasPieceInputSignature(context, options)) {
            // Piece tasks are allowed to cross clip revision drift only when their full semantic
            // input still matches the task snapshot. Clip-level tasks keep strict revision checks.
            const auto currentSignature = InferControllerHelper::buildSemanticSignature(
                context.taskType, *piece, context.singer);
            if (currentSignature.isEmpty())
                return drop("input-signature-unavailable", currentRevision);
            if (currentSignature != context.inputSignature)
                return drop("input-signature-mismatch", currentRevision);
            if (currentRevision != context.clipRevision) {
                logDecision(context, options.phase, Decision::Apply,
                            "revision-mismatch-input-signature-match", currentRevision);
            }
        } else if (currentRevision != context.clipRevision) {
            return drop("revision-mismatch", currentRevision);
        }
        if (options.checkEditSession && editSessionManager->hasActiveTransaction()) {
            const auto session = editSessionManager->activeSession();
            if (editSessionConflicts(session, context, resolution))
                return defer("edit-session-conflict", currentRevision);
        }
        return Decision::Apply;
    }
}
