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
    void onEditNotesLyrics(const QList<int> &notesId);
    void onInsertNote(Note *note);
    void onMoveNotes(const QList<int> &notesId, int deltaTick, int deltaKey);
    void onResizeNotesLeft(const QList<int> &notesId, int deltaTick);
    void onResizeNotesRight(const QList<int> &notesId, int deltaTick);
    void onAdjustPhoneme(const QList<int> &notesId, const QList<Phoneme> &phonemes);

private:
    SingingClip *m_clip = nullptr;
};



#endif // CLIPEDITVIEWCONTROLLER_H
