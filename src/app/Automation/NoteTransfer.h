#ifndef NOTETRANSFER_H
#define NOTETRANSFER_H

#include "ProjectAutomationDtos.h"

class Note;
class SingingClip;

namespace Automation {

    struct NoteTransferPayload {
        int sourceStart = 0;
        int sourceEnd = 0;
        QList<NoteDraftDto> notes;
        QList<ParamCurvesDraftDto> parameters;

        [[nodiscard]] bool isEmpty() const {
            return notes.isEmpty();
        }
    };

    [[nodiscard]] NoteTransferPayload captureNoteTransfer(const SingingClip &clip,
                                                          const QList<Note *> &notes);

    [[nodiscard]] QList<ParamCurvesDraftDto>
        mergeNoteTransferParameters(const SingingClip &target, const NoteTransferPayload &payload,
                                    int targetStart);

} // namespace Automation

#endif // NOTETRANSFER_H
