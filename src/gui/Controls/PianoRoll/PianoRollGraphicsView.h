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

public slots:
    void updateView();
    void onSelectedClipChanged(int trackIndex, int clipId);

private slots:
    void onCurrentClipPropertyChanged(DsClip *clip);

private:
    void paintEvent(QPaintEvent *event) override;
    PianoRollGraphicsScene *m_pianoRollScene;
    QVector<NoteGraphicsItem *> m_noteItems;
    bool m_oneSingingClipSelected = false;
    int m_trackIndex = -1;
    int m_clipId = -1;

    void reset();
    void insertNote(DsNote *dsNote, int index);
    void loadNotes(DsSingingClip *singingClip);
};

#endif // PIANOROLLGRAPHICSVIEW_H
