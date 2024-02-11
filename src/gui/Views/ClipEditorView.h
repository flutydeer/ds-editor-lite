//
// Created by fluty on 2024/2/10.
//

#ifndef CLIPEDITVIEW_H
#define CLIPEDITVIEW_H

#include "ClipEditorToolBarView.h"
#include "Controls/Base/TimelineView.h"
#include "Controls/PianoRoll/PianoRollGraphicsView.h"
#include "Model/Clip.h"

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
    void onEditModeChanged(PianoRollEditMode mode);
    void onPositionChanged(double tick);
    void onLastPositionChanged(double tick);
    void onRemoveSelectedNotes();
    void onEidtSelectedNotesLyrics();

private:
    Track *m_track = nullptr;
    Clip *m_clip = nullptr;
    DsSingingClip *m_singingClip = nullptr;
    ClipEditorToolBarView *m_toolbarView;
    PianoRollGraphicsScene *m_pianoRollScene;
    PianoRollGraphicsView *m_pianoRollView;
    TimelineView *m_timelineView;
    PianoRollEditMode m_mode;

    bool m_oneSingingClipSelected = false;

    void reset();
    void onClipPropertyChanged();
    void onNoteChanged(DsSingingClip::NoteChangeType type, int id, Note *note);
};



#endif //CLIPEDITVIEW_H
