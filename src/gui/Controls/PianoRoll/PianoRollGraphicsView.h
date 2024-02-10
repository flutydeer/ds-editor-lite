//
// Created by fluty on 2024/1/23.
//

#ifndef PIANOROLLGRAPHICSVIEW_H
#define PIANOROLLGRAPHICSVIEW_H

#include "Controls/Base/CommonGraphicsView.h"
#include "Model/AppModel.h"
#include "NoteGraphicsItem.h"
#include "PianoRollGraphicsScene.h"

class PianoRollGraphicsView final : public CommonGraphicsView {
    Q_OBJECT

public:
    explicit PianoRollGraphicsView();
    void setClip(DsTrack *track, DsClip *clip);

private slots:
    void onCurrentClipPropertyChanged(DsClip *clip);

private:
    void paintEvent(QPaintEvent *event) override;
    PianoRollGraphicsScene *m_pianoRollScene;
    QVector<NoteGraphicsItem *> m_noteItems;
    bool m_oneSingingClipSelected = false;
    DsTrack *m_track = nullptr;
    DsSingingClip *m_clip = nullptr;

    void reset();
    void insertNote(DsNote *dsNote, int index);
    void loadNotes(DsSingingClip *singingClip);
};

#endif // PIANOROLLGRAPHICSVIEW_H
