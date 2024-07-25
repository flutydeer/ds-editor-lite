//
// Created by fluty on 2023/11/14.
//

#ifndef DATASET_TOOLS_TRACKSGRAPHICSVIEW_H
#define DATASET_TOOLS_TRACKSGRAPHICSVIEW_H

#include "UI/Views/Common/TimeGraphicsView.h"

class TracksGraphicsScene;

class TracksGraphicsView final : public TimeGraphicsView {
    Q_OBJECT

public:
    explicit TracksGraphicsView(TracksGraphicsScene *scene, QWidget *parent = nullptr);
    void setQuantize(int quantize);

private slots:
    void onNewSingingClip() const;
    void onAddAudioClip();

private:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

    QAction *m_actionNewSingingClip;
    QAction *m_actionAddAudioClip;
    int m_trackIndex = -1;
    int m_tick = 0;
    int m_snappedTick = 0;
    int m_quantize = 16;
};

#endif // DATASET_TOOLS_TRACKSGRAPHICSVIEW_H
