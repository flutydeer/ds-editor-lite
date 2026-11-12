#ifndef DATASET_TOOLS_TRACKSGRAPHICSVIEW_H
#define DATASET_TOOLS_TRACKSGRAPHICSVIEW_H

#include "Interface/IAtomicAction.h"
#include "ClipSelectionDrag.h"
#include "TrackEditorContextMenuController.h"
#include "TracksGraphicsScene.h"
#include "UI/Views/Common/EditorSelectionUtils.h"
#include "UI/Views/Common/TimeGraphicsView.h"

#include <QSet>
#include <QUrl>

#include <optional>

class Menu;
class AbstractClipView;
class QGraphicsLineItem;
class QGraphicsPathItem;
class QGraphicsRectItem;
class TrackEditorBackgroundView;
class TracksGraphicsScene;

class TracksGraphicsView final : public TimeGraphicsView,
                                 public IAtomicAction,
                                 public ITrackEditorPreviewHost {
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
    void showTrackSplitPreview(const TrackSplitPreviewData &data) override;
    void clearTrackSplitPreview() override;
    [[nodiscard]] int snapStep(bool snapOff, int atTick = 0) const;

public slots:
    void setClipSelection(const QList<int> &selected) const;

signals:
    void contextMenuRequested(const TrackEditorMenuContext &context);
    // Emitted when an external file drag is dropped on the canvas. Phase 1
    // only resolves the drop slot; actual import is wired up in later phases.
    void externalDropRequested(const TrackDropSlot &slot, const QList<QUrl> &urls);

private slots:
    void onNewSingingClip() const;

private:
    bool event(QEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

    // --- EditorTouchTarget ---
    [[nodiscard]] BlankDragAction touchBlankDragAction() const override;
    void cancelTouchPointerInteraction() override;

    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void onEdgeAutoScrollFrame(const QPoint &clampedViewportPos,
                               Qt::KeyboardModifiers modifiers) override;
    void updateClipDragAt(const QPoint &viewportPos, Qt::KeyboardModifiers modifiers);
    void prepareForMovingOrResizingClip(const QMouseEvent *event, AbstractClipView *clipItem);
    AbstractClipView *findClipById(int id) const;
    bool updateClipSelection(AbstractClipView *clipItem, Qt::KeyboardModifiers modifiers);
    void applyClipSelection(const QList<int> &selected) const;
    void clearSelections() const;
    void resetActiveClips() const;
    void resetEditState();
    void syncClipSelectionToAppStatus() const;
    [[nodiscard]] QList<AbstractClipView *> selectedClipItems() const;
    // Visual order: track by track, clips inside a track by start. Shared by Shift range selection
    // and by the drag snapshots so both agree on what "between these two clips" means.
    [[nodiscard]] QList<int> orderedClipIds() const;
    void captureSelectionStates(QList<ClipSelectionDrag::ClipState> &states) const;
    void applyDragPlacements(const QList<ClipSelectionDrag::Placement> &placements) const;
    void restoreDragOriginals(const QSet<int> &skip = {}) const;
    [[nodiscard]] int tickAtSceneX(double sceneX) const;
    [[nodiscard]] int nudgeStep() const;
    void nudgeSelection(int deltaTicks, int deltaTracks) const;
    void previewAnchorTrackColor(int trackIndex) const;
    void restoreAnchorTrackColor() const;

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

    // Clip selection and the gesture in flight. The whole selection moves and trims as one block,
    // so the view keeps no per-clip drag state of its own: the planner owns it.
    EditorSelectionUtils::OrderedSelectionModel m_selectionModel;
    ClipSelectionDrag::Session m_dragSession;
    int m_dragAnchorId = -1;
    int m_dragTrackIndex = -1;
    bool m_movedBeforeMouseUp = false;
    QList<AbstractClipView *> m_pastePreviewClipViews;
    QGraphicsPathItem *m_splitPreviewItem = nullptr;

    // External file drag-and-drop state (Phase 1)
    bool m_externalDragActive = false;
    std::optional<TrackDropSlot> m_dropSlot;
    QGraphicsRectItem *m_dropHighlightItem = nullptr;
    QGraphicsLineItem *m_dropIndicatorLine = nullptr;
};

#endif // DATASET_TOOLS_TRACKSGRAPHICSVIEW_H
