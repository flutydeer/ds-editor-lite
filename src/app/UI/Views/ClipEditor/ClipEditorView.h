//
// Created by fluty on 2024/2/10.
//

#ifndef CLIPEDITVIEW_H
#define CLIPEDITVIEW_H

#include "Model/Track.h"
#include "PhonemeView.h"
#include "Interface/IClipEditorView.h"
#include "Interface/IPanel.h"

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

private:
    bool eventFilter(QObject *watched, QEvent *event) override;

    ClipEditorToolBarView *m_toolbarView;
    PianoRollGraphicsScene *m_pianoRollScene;
    PianoRollGraphicsView *m_pianoRollView;
    TimelineView *m_timelineView;
    PhonemeView *m_phonemeView;

    void reset();
    // void printParts();
    void afterSetActivated() override;
};



#endif // CLIPEDITVIEW_H
