//
// Created by fluty on 2024/2/10.
//

#ifndef CLIPEDITVIEW_H
#define CLIPEDITVIEW_H

#include "Interface/IClipEditorView.h"
#include "UI/Views/Common/PanelView.h"


class PianoKeyboardView;
class ClipEditorToolBarView;
class PhonemeView;
class PianoRollGraphicsScene;
class PianoRollGraphicsView;
class Track;
class TimelineView;
class Curve;
class Clip;
class SingingClip;
class AudioClip;

class ClipEditorView final : public PanelView, public IClipEditorView {
    Q_OBJECT
public:
    explicit ClipEditorView(QWidget *parent = nullptr);

    void centerAt(double tick, double keyIndex) override;
    void centerAt(double startTick, double length, double keyIndex) override;

public slots:
    void onModelChanged();
    void onSelectedClipChanged(Clip *clip);

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void moveToSingingClipState(SingingClip *clip) const;
    void moveToAudioClipState(AudioClip *clip) const;
    void moveToNullClipState() const;

    ClipEditorToolBarView *m_toolbarView;
    PianoRollGraphicsScene *m_pianoRollScene;
    PianoRollGraphicsView *m_pianoRollView;
    PianoKeyboardView *m_pianoKeyboardView;
    TimelineView *m_timelineView;
    PhonemeView *m_phonemeView;

    void reset();
    // void printParts();
};



#endif // CLIPEDITVIEW_H
