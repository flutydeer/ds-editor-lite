//
// Created by fluty on 2024/2/10.
//

#ifndef CLIPEDITVIEW_H
#define CLIPEDITVIEW_H

#include "ClipEditorToolBarView.h"
#include "PhonemeView.h"
#include "PianoRollGraphicsScene.h"
#include "PianoRollGraphicsView.h"
#include "Model/Track.h"
#include "Views/Common/TimelineView.h"

#include <QWidget>

class ClipEditorView final : public QWidget {
Q_OBJECT
public:
    explicit ClipEditorView(QWidget *parent = nullptr);

public slots:
    void onModelChanged();
    void onSelectedClipChanged(Track *track, Clip *clip);
    void onClipNameEdited(const QString &name);

private slots:
    void onClipChanged(Track::ClipChangeType type, int id, Clip *clip);
    void onEditModeChanged(PianoRollGlobal::PianoRollEditMode mode);
    void onPositionChanged(double tick);
    void onLastPositionChanged(double tick);
    void onRemoveSelectedNotes();
    void onEditSelectedNotesLyrics();
    void onDrawNoteCompleted(int start, int length, int keyIndex);
    void onMoveNotesCompleted(int deltaTick, int deltaKey);
    void onResizeNoteLeftCompleted(int noteId, int deltaTick);
    void onResizeNoteRightCompleted(int noteId, int deltaTick);

private:
    Track *m_track = nullptr;
    Clip *m_clip = nullptr;
    SingingClip *m_singingClip = nullptr;
    ClipEditorToolBarView *m_toolbarView;
    PianoRollGraphicsScene *m_pianoRollScene;
    PianoRollGraphicsView *m_pianoRollView;
    TimelineView *m_timelineView;
    PhonemeView *m_phonemeView;
    PianoRollEditMode m_mode;

    bool m_oneSingingClipSelected = false;

    void reset();
    void onClipPropertyChanged();
    void onNoteChanged(SingingClip::NoteChangeType type, int id, Note *note);
};



#endif //CLIPEDITVIEW_H
