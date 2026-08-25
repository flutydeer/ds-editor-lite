#include "DocumentObjectResolver.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

namespace Automation {

    AutomationResult<Track *> DocumentObjectResolver::track(DocumentSession &session,
                                                            const TrackId trackId) const {
        if (!trackId.isValid()) {
            return AutomationError::invalidArgument(QStringLiteral("track_id"),
                                                    QStringLiteral("Track ID is invalid"));
        }
        auto *model = session.model();
        if (!model) {
            AutomationError error;
            error.code = AutomationErrorCode::InternalError;
            error.message = QStringLiteral("Document session has no AppModel");
            return error;
        }
        if (auto *result = model->findTrackById(trackId.value()))
            return result;
        return AutomationError::notFound({ObjectKind::Track, trackId.value()},
                                         QStringLiteral("Track was not found"));
    }

    AutomationResult<ResolvedClip> DocumentObjectResolver::clip(DocumentSession &session,
                                                                const ClipId clipId) const {
        if (!clipId.isValid()) {
            return AutomationError::invalidArgument(QStringLiteral("clip_id"),
                                                    QStringLiteral("Clip ID is invalid"));
        }
        auto *model = session.model();
        if (!model) {
            AutomationError error;
            error.code = AutomationErrorCode::InternalError;
            error.message = QStringLiteral("Document session has no AppModel");
            return error;
        }
        int trackIndex = -1;
        auto *result = model->findClipById(clipId.value(), trackIndex);
        if (!result) {
            return AutomationError::notFound({ObjectKind::Clip, clipId.value()},
                                             QStringLiteral("Clip was not found"));
        }
        return ResolvedClip{result, model->tracks().value(trackIndex), trackIndex};
    }

    AutomationResult<ResolvedClip> DocumentObjectResolver::singingClip(DocumentSession &session,
                                                                       const ClipId clipId) const {
        auto result = clip(session, clipId);
        if (!result)
            return result.getError();
        if (result.get().clip->clipType() != Clip::Singing) {
            return AutomationError::wrongObjectType({ObjectKind::Clip, clipId.value()},
                                                    QStringLiteral("Clip is not a singing clip"));
        }
        return result;
    }

    AutomationResult<ResolvedClip> DocumentObjectResolver::audioClip(DocumentSession &session,
                                                                     const ClipId clipId) const {
        auto result = clip(session, clipId);
        if (!result)
            return result.getError();
        if (result.get().clip->clipType() != Clip::Audio) {
            return AutomationError::wrongObjectType({ObjectKind::Clip, clipId.value()},
                                                    QStringLiteral("Clip is not an audio clip"));
        }
        return result;
    }

    AutomationResult<ResolvedNote> DocumentObjectResolver::note(DocumentSession &session,
                                                                const ClipId clipId,
                                                                const NoteId noteId) const {
        auto resolvedClip = singingClip(session, clipId);
        if (!resolvedClip)
            return resolvedClip.getError();
        if (!noteId.isValid()) {
            return AutomationError::invalidArgument(QStringLiteral("note_id"),
                                                    QStringLiteral("Note ID is invalid"));
        }
        auto *singing = static_cast<SingingClip *>(resolvedClip.get().clip);
        auto *result = singing->findNoteById(noteId.value());
        if (!result) {
            return AutomationError::notFound({ObjectKind::Note, noteId.value()},
                                             QStringLiteral("Note was not found"));
        }
        return ResolvedNote{result, singing, resolvedClip.get().track};
    }

} // namespace Automation
