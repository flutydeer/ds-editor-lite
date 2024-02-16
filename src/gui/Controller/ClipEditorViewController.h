//
// Created by fluty on 2024/2/10.
//

#ifndef CLIPEDITVIEWCONTROLLER_H
#define CLIPEDITVIEWCONTROLLER_H

#include <QObject>

#include "Model/Clip.h"
#include "Model/AppModel.h"
#include "Utils/Singleton.h"

class ClipEditorViewController final : public QObject, public Singleton<ClipEditorViewController> {
    Q_OBJECT

public:
    void setCurrentSingingClip(SingingClip *clip);

public slots:
    void onClipPropertyChanged(const Clip::ClipCommonProperties &args);
    void onRemoveNotes(const QList<int> &notesId);
    void onEditNotesLyric(const QList<int> &notesId);
    void onInsertNote(Note *note);
    void onMoveNotes(const QList<int> &notesId, int deltaTick, int deltaKey);
    void onResizeNotesLeft(const QList<int> &notesId, int deltaTick);
    void onResizeNotesRight(const QList<int> &notesId, int deltaTick);
    void onAdjustPhoneme(const QList<int> &notesId, const QList<Phoneme> &phonemes);
    void onNoteSelectionChanged(const QList<int> &notesId, bool unselectOther);

    void onEditSelectedNotesLyric();
    void onRemoveSelectedNotes();
    // TODO: copy and paste selected notes

private:
    SingingClip *m_clip = nullptr;

    void editNotesLyric(const QList<Note *> &notes);
    void removeNotes(const QList<Note *> &notes);
};



#endif // CLIPEDITVIEWCONTROLLER_H
