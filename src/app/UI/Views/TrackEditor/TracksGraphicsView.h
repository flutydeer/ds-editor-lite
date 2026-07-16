//
// Created by fluty on 2023/11/14.
//

#ifndef DATASET_TOOLS_TRACKSGRAPHICSVIEW_H
#define DATASET_TOOLS_TRACKSGRAPHICSVIEW_H

#include "Interface/IAtomicAction.h"
#include "UI/Views/Common/TimeGraphicsView.h"

class Menu;
class AbstractClipView;
class TrackEditorBackgroundView;
class TracksGraphicsScene;

class TracksGraphicsView final : public TimeGraphicsView, public IAtomicAction {
    Q_OBJECT

public:
    explicit TracksGraphicsView(TracksGraphicsScene *scene, const QWidget *parent = nullptr);
    void setSnapGrid(TrackEditorBackgroundView *grid);
    [[nodiscard]] QList<int> selectedClipsId() const;

    void discardAction() override;
    void commitAction() override;

private slots:
    void onNewSingingClip() const;
    void onAddAudioClip();
    void onDeleteTriggered() const;
    static void onExtractMidiTriggered(int clipId);
    void onRelocateAudioTriggered(int clipId);

private:
    enum MouseMoveBehavior { Move, ResizeRight, ResizeLeft, None };

    bool event(QEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void prepareForMovingOrResizingClip(const QMouseEvent *event, AbstractClipView *clipItem);
    AbstractClipView *findClipById(int id) const;
    void clearSelections() const;
    void resetActiveClips() const;
    void resetEditState();
    void syncClipSelectionToAppStatus() const;
    [[nodiscard]] int snapStep(bool snapOff) const;
    [[nodiscard]] QList<AbstractClipView *> selectedClipItems() const;

    TracksGraphicsScene *m_scene;
    QAction *m_actionNewSingingClip;
    QAction *m_actionAddAudioClip;
    int m_trackIndex = -1;
    int m_tick = 0;
    TrackEditorBackgroundView *m_snapGrid = nullptr;

    MouseMoveBehavior m_mouseMoveBehavior = None;
    QPointF m_mouseDownPos;
    int m_mouseDownStart = 0;
    int m_mouseDownClipStart = 0;
    int m_mouseDownLength = 0;
    int m_mouseDownClipLen = 0;
    bool m_movedBeforeMouseUp = false;
    int m_mouseDownTrackIndex = -1;
    int m_mouseDownColorIndex = 0;
    bool m_tempQuantizeOff = false;
    AbstractClipView *m_currentEditingClip = nullptr;
    QList<AbstractClipView *> m_pastePreviewClipViews;
    void clearPastePreviewClipViews();
};

#endif // DATASET_TOOLS_TRACKSGRAPHICSVIEW_H
