//
// Created by fluty on 2023/11/14.
//

#ifndef DATASET_TOOLS_TRACKSGRAPHICSVIEW_H
#define DATASET_TOOLS_TRACKSGRAPHICSVIEW_H

#include "UI/Views/Common/TimeGraphicsView.h"

class CMenu;
class AbstractClipView;
class TracksGraphicsScene;

class TracksGraphicsView final : public TimeGraphicsView {
    Q_OBJECT

public:
    explicit TracksGraphicsView(TracksGraphicsScene *scene, const QWidget *parent = nullptr);
    void setQuantize(int quantize);
    [[nodiscard]] QList<int> selectedClipsId() const;

private slots:
    void onNewSingingClip() const;
    void onAddAudioClip();
    void onDeleteTriggered() const;
    static void onExtractMidiTriggered(int clipId);

private:
    enum MouseMoveBehavior { Move, ResizeRight, ResizeLeft, None };

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void prepareForMovingOrResizingClip(const QMouseEvent *event, AbstractClipView *clipItem);
    AbstractClipView *findClipById(int id) const;
    void clearSelections() const;
    void resetActiveClips() const;
    [[nodiscard]] QList<AbstractClipView *> selectedClipItems() const;

    TracksGraphicsScene *m_scene;
    CMenu *m_backgroundMenu = nullptr;
    QAction *m_actionNewSingingClip;
    QAction *m_actionAddAudioClip;
    int m_trackIndex = -1;
    int m_tick = 0;
    int m_snappedTick = 0;
    int m_quantize = 16;

    MouseMoveBehavior m_mouseMoveBehavior = None;
    QPointF m_mouseDownPos;
    int m_mouseDownStart = 0;
    int m_mouseDownClipStart = 0;
    int m_mouseDownLength = 0;
    int m_mouseDownClipLen = 0;
    bool m_movedBeforeMouseUp = false;
    // int m_mouseDownTrackIndex = -1;
    bool m_tempQuantizeOff = false;
    AbstractClipView *m_currentEditingClip = nullptr;
};

#endif // DATASET_TOOLS_TRACKSGRAPHICSVIEW_H
