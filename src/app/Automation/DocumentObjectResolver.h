#ifndef DOCUMENTOBJECTRESOLVER_H
#define DOCUMENTOBJECTRESOLVER_H

#include "DocumentSession.h"

class AudioClip;
class Clip;
class Note;
class SingingClip;
class Track;

namespace Automation {

    struct ResolvedClip {
        Clip *clip = nullptr;
        Track *track = nullptr;
        qsizetype trackIndex = -1;
    };

    struct ResolvedNote {
        Note *note = nullptr;
        SingingClip *clip = nullptr;
        Track *track = nullptr;
    };

    class DocumentObjectResolver final {
    public:
        [[nodiscard]] AutomationResult<Track *> track(DocumentSession &session,
                                                      TrackId trackId) const;
        [[nodiscard]] AutomationResult<ResolvedClip> clip(DocumentSession &session,
                                                          ClipId clipId) const;
        [[nodiscard]] AutomationResult<ResolvedClip> singingClip(DocumentSession &session,
                                                                 ClipId clipId) const;
        [[nodiscard]] AutomationResult<ResolvedClip> audioClip(DocumentSession &session,
                                                               ClipId clipId) const;
        [[nodiscard]] AutomationResult<ResolvedNote> note(DocumentSession &session, ClipId clipId,
                                                          NoteId noteId) const;
    };

} // namespace Automation

#endif // DOCUMENTOBJECTRESOLVER_H
