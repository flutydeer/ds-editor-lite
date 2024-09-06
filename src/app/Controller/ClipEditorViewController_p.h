//
// Created by fluty on 24-7-21.
//

#ifndef CLIPEDITORVIEWCONTROLLER_P_H
#define CLIPEDITORVIEWCONTROLLER_P_H

#include "Model/ClipboardDataModel/NotesParamsInfo.h"

class Note;
class Clip;
class IClipEditorView;
class ClipEditorViewController;

class ClipEditorViewControllerPrivate {
    Q_DECLARE_PUBLIC(ClipEditorViewController);

public:
    explicit ClipEditorViewControllerPrivate(ClipEditorViewController *q) : q_ptr(q) {
    }

    IClipEditorView *m_view = nullptr;
    Clip *m_clip = nullptr;

    void editNotesLyric(const QList<Note *> &notes) const;
    void removeNotes(const QList<Note *> &notes) const;
    [[nodiscard]] NotesParamsInfo buildNoteParamsInfo() const;
    [[nodiscard]] static QList<Note *> selectedNotesFromId(const QList<int> &notesId, SingingClip *clip) ;

private:
    ClipEditorViewController *q_ptr;
};

#endif // CLIPEDITORVIEWCONTROLLER_P_H
