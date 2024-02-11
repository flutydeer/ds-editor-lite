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
    void onClipNameEdited(const QString &name);

private slots:
    void handleClipChanged(DsTrack::ClipChangeType type, int id, DsClip *clip);
    void onEditModeChanged(PianoRollEditMode mode);

private:
    DsTrack *m_track = nullptr;
    DsClip *m_clip = nullptr;
    ClipEditorToolBarView *m_toolbarView;
    PianoRollGraphicsScene *m_pianoRollScene;
    PianoRollGraphicsView *m_graphicsView;
    PianoRollEditMode m_mode;

    bool m_oneSingingClipSelected = false;

    void reset();

    void handleClipPropertyChange();
};



#endif //CLIPEDITVIEW_H
