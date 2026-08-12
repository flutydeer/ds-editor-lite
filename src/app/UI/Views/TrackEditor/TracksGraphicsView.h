#ifndef DATASET_TOOLS_TRACKSGRAPHICSVIEW_H
#define DATASET_TOOLS_TRACKSGRAPHICSVIEW_H

#include "Interface/IAtomicAction.h"
#include "AudioClipDragState.h"
#include "TrackEditorContextMenuController.h"
#include "TracksGraphicsScene.h"
#include "UI/Views/Common/TimeGraphicsView.h"

#include <QUrl>

#include <optional>

class Menu;
class AbstractClipView;
class QGraphicsLineItem;
class QGraphicsRectItem;
class TrackEditorBackgroundView;
class TracksGraphicsScene;

class TracksGraphicsView final : public TimeGraphicsView,
                                 public IAtomicAction,
                                 public ITrackPastePreviewHost {
    Q_OBJECT
    Q_PROPERTY(QColor selectedTrackColor READ selectedTrackColor WRITE setSelectedTrackColor)
    Q_PROPERTY(QColor clipSelectedBorderColor READ clipSelectedBorderColor WRITE
                   setClipSelectedBorderColor)
    Q_PROPERTY(QColor dropHighlightColor READ dropHighlightColor WRITE setDropHighlightColor)
    Q_PROPERTY(QColor dropIndicatorColor READ dropIndicatorColor WRITE setDropIndicatorColor)

public:
    explicit TracksGraphicsView(TracksGraphicsScene *scene, QWidget *parent = nullptr);
    void setSnapGrid(TrackEditorBackgroundView *grid);
    [[nodiscard]] QList<int> selectedClipsId() const;

    [[nodiscard]] double sceneXForTick(double tick) const {
        return TimeGraphicsView::tickToSceneX(tick);
    }

    void discardAction() override;
    void commitAction() override;
    void showTrackPastePreview(const TrackPastePreviewData &data, int previewTick,
                               int baseTrackIndex) override;
    void clearTrackPastePreview() override;
    [[nodiscard]] int snapStep(bool snapOff, int atTick = 0) const;

signals:
    void contextMenuRequested(const TrackEditorMenuContext &context);
    // Emitted when an external file drag is dropped on the canvas. Phase 1
    // only resolves the drop slot; actual import is wired up in later phases.
    void externalDropRequested(const TrackDropSlot &slot, const QList<QUrl> &urls);

private slots:
    void onNewSingingClip() const;

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
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void onEdgeAutoScrollFrame(const QPoint &clampedViewportPos,
                               Qt::KeyboardModifiers modifiers) override;
    void updateClipDragAt(const QPoint &viewportPos, Qt::KeyboardModifiers modifiers);
    void prepareForMovingOrResizingClip(const QMouseEvent *event, AbstractClipView *clipItem);
    AbstractClipView *findClipById(int id) const;
    void clearSelections() const;
    void resetActiveClips() const;
    void resetEditState();
    void syncClipSelectionToAppStatus() const;
    [[nodiscard]] QList<AbstractClipView *> selectedClipItems() const;

    // --- External file drag-and-drop (Phase 1) ---
    // Resolves the drop slot at the given viewport position, hitting either a
    // real track or the virtual append slot. Returns nullopt outside the
    // canvas (e.g. over the timeline ruler).
    [[nodiscard]] std::optional<TrackDropSlot> dropSlotAt(const QPoint &viewportPos) const;
    void updateExternalDropOverlay(const QPoint &viewportPos);
    void endExternalDropOverlay();
    void ensureDropOverlayItems();
    void updateDropOverlayGeometry();

    [[nodiscard]] QColor selectedTrackColor() const;
    void setSelectedTrackColor(const QColor &color);
    [[nodiscard]] QColor clipSelectedBorderColor() const;
    void setClipSelectedBorderColor(const QColor &color);
    [[nodiscard]] QColor dropHighlightColor() const;
    void setDropHighlightColor(const QColor &color);
    [[nodiscard]] QColor dropIndicatorColor() const;
    void setDropIndicatorColor(const QColor &color);

    TracksGraphicsScene *m_scene;
    int m_trackIndex = -1;
    int m_tick = 0;
    TrackEditorBackgroundView *m_snapGrid = nullptr;
    // Applied to the snap grid when it is attached via setSnapGrid
    QColor m_selectedTrackColor = {0x31, 0x35, 0x3F};
    // External drop overlay colors (theme-injected via QSS)
    QColor m_dropHighlightColor{0xA9, 0xC4, 0xFF, 0x50};
    QColor m_dropIndicatorColor{200, 200, 200};

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
    std::optional<AudioClipDragState> m_audioDragState;
    AbstractClipView *m_currentEditingClip = nullptr;
    QList<AbstractClipView *> m_pastePreviewClipViews;

    // External file drag-and-drop state (Phase 1)
    bool m_externalDragActive = false;
    std::optional<TrackDropSlot> m_dropSlot;
    QGraphicsRectItem *m_dropHighlightItem = nullptr;
    QGraphicsLineItem *m_dropIndicatorLine = nullptr;
};

#endif // DATASET_TOOLS_TRACKSGRAPHICSVIEW_H
