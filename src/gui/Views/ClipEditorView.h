//
// Created by fluty on 2024/2/10.
//

#ifndef CLIPEDITVIEW_H
#define CLIPEDITVIEW_H

#include "ClipEditorToolBarView.h"
#include "Controls/PianoRoll/PianoRollGraphicsView.h"
#include "Model/DsClip.h"

#include <QWidget>

class ClipEditorView final : public QWidget {
Q_OBJECT
public:
    explicit ClipEditorView(QWidget *parent = nullptr);

public slots:
    void onModelChanged();
    void onSelectedClipChanged(DsTrack *track, DsClip *clip);

private:
    DsTrack *m_track = nullptr;
    DsClip *m_clip = nullptr;
    ClipEditorToolBarView *m_toolbarView;
    PianoRollGraphicsView *m_pianoRollView;
};



#endif //CLIPEDITVIEW_H
