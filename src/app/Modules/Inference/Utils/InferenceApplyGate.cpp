//
// Created by FlutyDeer on 2026/6/7.
//

#include "InferenceApplyGate.h"

#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/InferPiece.h"
#include "Model/AppModel/Note.h"
#include "Model/AppModel/SingingClip.h"
#include "Model/AppStatus/AppStatus.h"

#include <QDebug>

namespace InferenceApplyGate {
    void logDrop(const InferenceTaskContext &context, const QString &phase, const QString &reason,
                 const quint64 currentRevision) {
        qInfo() << "Drop stale" << context.taskType << "result:"
                << "clipId:" << context.clipId << "pieceId:" << context.pieceId
                << "taskId:" << context.taskId << "taskRevision:" << context.clipRevision
                << "currentRevision:" << currentRevision << "phase:" << phase
                << "reason:" << reason;
    }

    Decision resolve(const InferenceTaskContext &context, InferenceTaskResolution &resolution,
                     const Options &options) {
        const auto drop = [&resolution, &context, &options](const QString &reason,
                                                            const quint64 currentRevision = 0) {
            logDrop(context, options.phase, reason, currentRevision);
            resolution = {};
            resolution.dropReason = reason;
            return Decision::Drop;
        };

        if (context.clipId < 0)
            return drop("context-missing");

        if (options.checkActiveEditSession &&
            appStatus->currentEditObject != AppStatus::EditObjectType::None) {
            return drop("active-edit-session");
        }

        const auto clipObject = appModel->findClipById(context.clipId);
        if (!clipObject)
            return drop("clip-not-found");

        const auto clip = dynamic_cast<SingingClip *>(clipObject);
        if (!clip)
            return drop("not-singing-clip");

        const auto currentRevision = clip->inferenceRevision();
        if (currentRevision != context.clipRevision)
            return drop("revision-mismatch", currentRevision);

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
        }

        if (options.checkSingerSpeaker &&
            (clip->singerIdentifier() != context.singer || clip->speakerId() != context.speaker)) {
            return drop("clip-singer-speaker-mismatch", currentRevision);
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
        return Decision::Apply;
    }
}
