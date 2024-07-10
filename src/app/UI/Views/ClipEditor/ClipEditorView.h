//
// Created by fluty on 2024/2/10.
//

#ifndef CLIPEDITVIEW_H
#define CLIPEDITVIEW_H

#include "Global/ClipEditorGlobal.h"
#include "Model/Track.h"
#include "PhonemeView.h"
#include "Interface/IClipEditorView.h"
#include "Model/Clip.h"
#include "Model/Params.h"

class ClipEditorToolBarView;
class PhonemeView;
class PianoRollGraphicsScene;
class PianoRollGraphicsView;
class Track;
class TimelineView;
class Curve;

class ClipEditorView final : public QWidget, public IClipEditorView {
    Q_OBJECT
public:
    explicit ClipEditorView(QWidget *parent = nullptr);

    void centerAt(double tick, double keyIndex) override;
    void centerAt(double startTick, double length, double keyIndex) override;

public slots:
    void onModelChanged();
    void onSelectedClipChanged(Track *track, Clip *clip);
    void onClipNameEdited(const QString &name);

private slots:
    void onClipChanged(Track::ClipChangeType type, int id, Clip *clip);
    void onEditModeChanged(ClipEditorGlobal::PianoRollEditMode mode);
    void onPositionChanged(double tick);
    void onLastPositionChanged(double tick);
    void onRemoveSelectedNotes();
    void onEditSelectedNotesLyrics();
    void onDrawNoteCompleted(int start, int length, int keyIndex);
    void onMoveNotesCompleted(int deltaTick, int deltaKey);
    void onResizeNoteLeftCompleted(int noteId, int deltaTick);
    void onResizeNoteRightCompleted(int noteId, int deltaTick);
    void onAdjustPhonemeCompleted(PhonemeView::PhonemeViewModel *phonemeViewModel);
    void onPianoRollSelectionChanged();
    void onPitchEdited(const OverlapableSerialList<Curve> &curves);
    void onParamChanged(ParamBundle::ParamName paramName, Param::ParamType paramType);

private:
    Track *m_track = nullptr;
    Clip *m_clip = nullptr;
    SingingClip *m_singingClip = nullptr;
    ClipEditorToolBarView *m_toolbarView;
    PianoRollGraphicsScene *m_pianoRollScene;
    PianoRollGraphicsView *m_pianoRollView;
    TimelineView *m_timelineView;
    PhonemeView *m_phonemeView;
    ClipEditorGlobal::PianoRollEditMode m_mode = ClipEditorGlobal::Select;

    bool m_oneSingingClipSelected = false;

    void reset();
    void onClipPropertyChanged();
    void onNoteListChanged(SingingClip::NoteChangeType type, int id, Note *note);
    void onNotePropertyChanged(SingingClip::NotePropertyType type, Note *note);
    void onNoteSelectionChanged();
    void printParts();
};



#endif // CLIPEDITVIEW_H
