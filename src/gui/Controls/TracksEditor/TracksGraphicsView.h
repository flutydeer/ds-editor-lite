//
// Created by fluty on 2023/11/14.
//

#ifndef DATASET_TOOLS_TRACKSGRAPHICSVIEW_H
#define DATASET_TOOLS_TRACKSGRAPHICSVIEW_H

#include "../Base/CommonGraphicsView.h"
#include "Model/AppModel.h"

class TracksGraphicsView final : public CommonGraphicsView {
    Q_OBJECT

public:
    explicit TracksGraphicsView();

signals:
    void addSingingClipTriggered(int trackIndex, int tick);
    void addAudioClipTriggered(int trackIndex, int tick);

private:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

    QAction *m_actionNewSingingClip;
    QAction *m_actionAddAudioClip;
    int m_trackIndx = -1;
    int m_tick = 0;
    int m_snappedTick = 0;
};

#endif // DATASET_TOOLS_TRACKSGRAPHICSVIEW_H
