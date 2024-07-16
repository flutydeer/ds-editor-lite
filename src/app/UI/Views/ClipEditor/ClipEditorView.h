//
// Created by fluty on 2024/2/10.
//

#ifndef CLIPEDITVIEW_H
#define CLIPEDITVIEW_H

#include "Global/ClipEditorGlobal.h"
#include "Model/Track.h"
#include "PhonemeView.h"
#include "Interface/IClipEditorView.h"
#include "Interface/IPanel.h"
#include "Model/Clip.h"
#include "Model/Params.h"

class ClipEditorToolBarView;
class PhonemeView;
class PianoRollGraphicsScene;
class PianoRollGraphicsView;
class Track;
class TimelineView;
class Curve;

class ClipEditorView final : public QWidget, public IClipEditorView, public IPanel {
    Q_OBJECT
public:
    explicit ClipEditorView(QWidget *parent = nullptr);
    [[nodiscard]] AppGlobal::PanelType panelType() const override {
        return AppGlobal::ClipEditor;
    }

    void centerAt(double tick, double keyIndex) override;
    void centerAt(double startTick, double length, double keyIndex) override;

public slots:
    void onModelChanged();
    void onSelectedClipChanged(Clip *clip);

private slots:
    void onEditModeChanged(ClipEditorGlobal::PianoRollEditMode mode);
    void onParamChanged(ParamBundle::ParamName paramName, Param::ParamType paramType);
    void onClipPropertyChanged();

private:
    bool eventFilter(QObject *watched, QEvent *event) override;

    Clip *m_clip = nullptr;
    SingingClip *m_singingClip = nullptr;
    ClipEditorToolBarView *m_toolbarView;
    PianoRollGraphicsScene *m_pianoRollScene;
    PianoRollGraphicsView *m_pianoRollView;
    TimelineView *m_timelineView;
    PhonemeView *m_phonemeView;
    ClipEditorGlobal::PianoRollEditMode m_mode = ClipEditorGlobal::Select;
    QList<Note *> m_notes;

    bool m_oneSingingClipSelected = false;

    void reset();
    void onNoteListChanged(SingingClip::NoteChangeType type, int id, Note *note);
    void onNotePropertyChanged(Note::NotePropertyType type, Note *note);
    void onNoteSelectionChanged();
    // void printParts();
    void afterSetActivated() override;
    void updateStyleSheet();
};



#endif // CLIPEDITVIEW_H
