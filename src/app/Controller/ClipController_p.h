//
// Created by fluty on 24-7-21.
//

#ifndef CLIPCONTROLLER_P_H
#define CLIPCONTROLLER_P_H

#include "Model/ClipboardDataModel/NotesParamsInfo.h"

class Note;
class Clip;
class IClipEditorView;
class ClipController;

class ClipControllerPrivate {
    Q_DECLARE_PUBLIC(ClipController);

public:
    explicit ClipControllerPrivate(ClipController *q) : q_ptr(q) {
    }

    IClipEditorView *m_view = nullptr;
    Clip *m_clip = nullptr;

    void editNotesLyric(const QList<Note *> &notes) const;
    void removeNotes(const QList<Note *> &notes) const;
    NotesParamsInfo buildNoteParamsInfo() const;
    static QList<Note *> selectedNotesFromId(const QList<int> &notesId, const SingingClip *clip);

private:
    ClipController *q_ptr;
};

#endif // CLIPCONTROLLER_P_H
