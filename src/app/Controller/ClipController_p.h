#ifndef CLIPCONTROLLER_P_H
#define CLIPCONTROLLER_P_H

#include "Model/ClipboardDataModel/NotesParamsInfo.h"
#include <QPointer>

class Note;
class Clip;
class ClipController;

class ClipControllerPrivate {
    Q_DECLARE_PUBLIC(ClipController);

public:
    explicit ClipControllerPrivate(ClipController *q) : q_ptr(q) {
    }

    QPointer<Clip> m_clip;

    [[nodiscard]] Automation::AutomationResult<NotesParamsInfo> buildNoteParamsInfo() const;
    [[nodiscard]] static QList<Note *> selectedNotesFromId(const QList<int> &notesId,
                                                           const SingingClip *clip);

private:
    ClipController *q_ptr;
};

#endif // CLIPCONTROLLER_P_H
