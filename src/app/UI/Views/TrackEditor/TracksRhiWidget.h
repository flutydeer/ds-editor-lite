#ifndef TRACKSRHIWIDGET_H
#define TRACKSRHIWIDGET_H

#include "AudioWaveformSampler.h"
#include "ClipSelectionDrag.h"
#include "Global/AppGlobal.h"
#include "Interface/EditorViewState.h"
#include "Model/AppStatus/EditorPreview.h"
#include "TrackEditorContextMenuController.h"
#include "TracksGraphicsScene.h"
#include "UI/Views/Common/EditorGlyphAtlas.h"
#include "UI/Views/Common/EditorPenTarget.h"
#include "UI/Views/Common/EditorRhiWidget.h"
#include "UI/Views/Common/EditorSelectionUtils.h"
#include "UI/Views/Common/EditorTouchTarget.h"
#include "UI/Views/Common/EditorViewportController.h"
#include "UI/Views/Common/EdgeAutoScroller.h"

#include <lite/History/HistoryFocus.h>
#include <lite/ProjectModel/AppModel/Clip.h>

#include <QHash>
#include <QTimer>
#include <QUrl>

#include <memory>
#include <optional>

class QContextMenuEvent;
class QEvent;
class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;
class QHideEvent;
class QKeyEvent;
class QMouseEvent;
class EditorPenController;
class EditorRhiScrollBarController;
class EditorTouchController;
class EditorWheelController;
class ClipTitleInlineEdit;
enum class EditSessionEndReason;
class QResizeEvent;
class QShowEvent;
class QWheelEvent;

class TracksRhiWidget final : public EditorRhiWidget,
                              public ITrackEditorPreviewHost,
                              public EditorTouchTarget,
                              public EditorPenTarget {
    Q_OBJECT
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor)
    Q_PROPERTY(QColor barLineColor READ barLineColor WRITE setBarLineColor)
    Q_PROPERTY(QColor beatLineColor READ beatLineColor WRITE setBeatLineColor)
    Q_PROPERTY(QColor commonLineColor READ commonLineColor WRITE setCommonLineColor)
    Q_PROPERTY(
        QColor playPosIndicatorColor READ playPosIndicatorColor WRITE setPlayPosIndicatorColor)
    Q_PROPERTY(QColor lastPlayPosIndicatorColor READ lastPlayPosIndicatorColor WRITE
                   setLastPlayPosIndicatorColor)
    Q_PROPERTY(QColor selectedTrackColor READ selectedTrackColor WRITE setSelectedTrackColor)
    Q_PROPERTY(QColor clipSelectedBorderColor READ clipSelectedBorderColor WRITE
                   setClipSelectedBorderColor)
    Q_PROPERTY(
        QColor rubberBandBorderColor READ rubberBandBorderColor WRITE setRubberBandBorderColor)
    Q_PROPERTY(QColor rubberBandFillColor READ rubberBandFillColor WRITE setRubberBandFillColor)
    Q_PROPERTY(QColor dropHighlightColor READ dropHighlightColor WRITE setDropHighlightColor)
    Q_PROPERTY(QColor dropIndicatorColor READ dropIndicatorColor WRITE setDropIndicatorColor)

public:
    explicit TracksRhiWidget(QWidget *parent = nullptr);
    ~TracksRhiWidget() override;

    [[nodiscard]] TrackPanelViewState viewState() const;
    bool centerAt(double tick, double trackIndex);
    bool setViewScale(double horizontalScale, double verticalScale);
    [[nodiscard]] HistoryFocusVisibility focusVisibility(const HistoryFocus &focus) const;
    bool revealFocus(const HistoryFocus &focus, bool animated = true);
    [[nodiscard]] QRectF logicalVisibleRect() const;
    [[nodiscard]] double scaleX() const;
    [[nodiscard]] double scaleY() const;
    [[nodiscard]] double startTick() const;
    [[nodiscard]] double endTick() const;
    // Left margin in screen pixels before tick 0, forwarded to the viewport
    // controller so the playhead indicator never clips at the left edge.
    void setLeftMarginPx(double px);
    void showTrackPastePreview(const TrackPastePreviewData &data, int previewTick,
                               int baseTrackIndex) override;
    void clearTrackPastePreview() override;
    void showTrackSplitPreview(const TrackSplitPreviewData &data) override;
    void clearTrackSplitPreview() override;
    // WYSIWYG snap step at `tick` for the current zoom level.
    [[nodiscard]] int snapStep(int tick) const;
    // Commit an open inline clip title rename. No-op when nothing is renamed.
    void finishClipTitleEditing();
    // Drop an open rename without writing the model.
    void cancelClipTitleEditing();

public slots:
    void setSceneLength(int tick);
    void setPlaybackPosition(double tick);
    void setLastPlaybackPosition(double tick);
    void setAutoPageTurn(bool enabled);
    void onWheelHorScale(QWheelEvent *event);
    void onWheelVerScale(QWheelEvent *event);
    void onWheelHorScroll(QWheelEvent *event);
    void onWheelVerScroll(QWheelEvent *event);
    void setVerticalOffset(double value);
    void scheduleSnapshot();

signals:
    void scaleChanged(double horizontal, double vertical);
    void visibleRectChanged(const QRectF &rect);
    void sizeChanged(QSize size);
    void timeRangeChanged(double startTick, double endTick);
    void verticalOffsetChanged(double value);
    void setPositionTriggered(double tick);
    void contextMenuRequested(const TrackEditorMenuContext &context);
    void autoPageTurnAvailabilityChanged(bool available);
    // Emitted when an external file drag is dropped on the canvas. Phase 1
    // only resolves the drop slot; actual import is wired up in later phases.
    void externalDropRequested(const TrackDropSlot &slot, const QList<QUrl> &urls);

protected:
    bool event(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void onRhiReady() override;
    void onDevicePixelRatioChanged() override;

    // --- EditorTouchTarget ---
    void stopTouchViewportAnimation() override;
    void panTouchViewportBy(const QPointF &deltaPixels) override;
    void zoomTouchViewportBy(double horizontalFactor, double verticalFactor,
                             const QPointF &anchor) override;
    [[nodiscard]] ContentHit touchContentAt(const QPointF &viewportPosition) const override;
    [[nodiscard]] BlankDragAction touchBlankDragAction() const override;
    void cancelTouchPointerInteraction() override;
    // An open inline rename owns the touches that land on it.
    [[nodiscard]] bool touchRelayTextBegin(const QPointF &viewportPosition) override;
    void touchRelayTextMove(const QPointF &viewportPosition) override;
    void touchRelayTextEnd(const QPointF &viewportPosition) override;
    void touchRelayTextCancel() override;

    // --- EditorPenTarget ---
    // The arrangement canvas has no tool of its own and nothing an eraser could
    // do, so an erase stroke is swallowed before it reaches the interaction
    // layer — the same answer the legacy arrangement view gives.
    [[nodiscard]] EditorPenEraser penEraserAction() const override;

private:
    enum class DragMode { None, Move, ResizeLeft, ResizeRight, RectSelect };

    using NoteSnapshot = EditorPreview::Note;

    struct ClipSnapshot {
        int id = -1;
        int trackIndex = -1;
        int colorIndex = 0;
        IClip::ClipType type = IClip::Generic;
        int contentStartTick = 0;
        int visibleStartTick = 0;
        int visibleEndTick = 0;
        QRectF modelPhysicalRect;
        QRectF paintPhysicalRect;
        QString title;
        // An open inline rename hides the painted title: the overlay is the
        // only place the old name shows.
        bool titleEditing = false;
        bool selected = false;
        bool active = false;
        bool pastePreview = false;
        // Audio clips only. Loading draws the placeholder, Missing draws the missing-file hint and
        // Loaded samples the waveform, so the canvas no longer combines PathStatus with task state
        // on its own.
        AppGlobal::AudioClipLoadState loadState = AppGlobal::AudioClipLoadState::Loaded;
        QVector<NoteSnapshot> notes;
        AudioWaveformSampler::Result waveform;
    };

    void rebuildSnapshot();
    void rebuildModelConnections();
    void appendGrid(EditorRhiFrameData &frame, double dpr) const;
    void appendClips(EditorRhiFrameData &frame, double dpr);
    void appendClip(EditorRhiFrameData &frame, const ClipSnapshot &clip, double dpr);
    void appendLastPlaybackIndicator(EditorRhiFrameData &frame, double dpr) const;
    void updatePlaybackOverlay();
    void appendDropOverlay(EditorRhiFrameData &frame, double dpr) const;
    void appendSplitPreviewOverlay(EditorRhiFrameData &frame, double dpr) const;
    [[nodiscard]] ClipSnapshot buildClipSnapshot(const Clip *clip, int trackIndex,
                                                 double dpr) const;
    [[nodiscard]] QRectF clipModelPhysicalRect(const Clip::ClipCommonProperties &properties,
                                               int trackIndex, double dpr) const;
    [[nodiscard]] static QRectF clipPreviewRect(const ClipSnapshot &clip, double dpr);
    [[nodiscard]] AudioWaveformSampler::Result sampleAudioWaveform(AudioWaveformSampler &sampler,
                                                                   const AudioInfoModel &audioInfo,
                                                                   const ClipSnapshot &clip,
                                                                   double dpr) const;
    [[nodiscard]] const ClipSnapshot *hitTest(const QPointF &viewportPosition) const;
    // The band that carries the clip title, in viewport coordinates. Empty when
    // the clip is too short to draw its title as its own strip.
    [[nodiscard]] QRectF titleBandViewportRect(const ClipSnapshot &clip) const;
    void beginClipTitleEdit(const ClipSnapshot &clip);
    [[nodiscard]] int trackIndexAt(const QPointF &viewportPosition) const;
    [[nodiscard]] int tickAt(const QPointF &viewportPosition) const;
    [[nodiscard]] int snapTick(int tick) const;
    void beginClipDrag(const ClipSnapshot &clip, const QMouseEvent *event);
    void updateDrag(const QPointF &position, Qt::KeyboardModifiers modifiers);
    void updateRubberBandSelection(const QPointF &position);
    void commitDrag();
    void discardDrag();
    void finishDragSession(EditSessionEndReason reason);
    bool updateClipSelection(const ClipSnapshot &clip, Qt::KeyboardModifiers modifiers);
    void syncSelection(const QList<int> &ids, int preferredTrack = -1);
    void updateCursor(const QPointF &position);
    void handleAutoPageTurn();
    void updateAutoPageTurnAvailability();
    void updateScrollBars();
    [[nodiscard]] int effectiveSceneLength() const;
    void setSceneLengthExtension(int ticks);
    [[nodiscard]] Clip::ClipCommonProperties previewOrModelProperties(const Clip *clip) const;
    [[nodiscard]] Qt::Orientations dragAutoScrollAxes() const;
    void prepareDragAutoScroll(const QPointF &pressPosition);
    void disarmDragAutoScroll();
    void updateDragAutoScrollState(const QPointF &pointerPosition);
    void onDragAutoScrollFrame(double dtMs);

    // --- Clip selection gestures ---
    [[nodiscard]] QList<int> orderedClipIds() const;
    void captureSelectionStates(QList<ClipSelectionDrag::ClipState> &states) const;
    void applyDragPlacements(const QList<ClipSelectionDrag::Placement> &placements);
    void clearDragPlacements();
    void previewAnchorTrackColor(int trackIndex) const;
    void restoreAnchorTrackColor() const;
    [[nodiscard]] int nudgeStep() const;
    void nudgeSelection(int deltaTicks, int deltaTracks) const;

    // --- External file drag-and-drop (Phase 1) ---
    // Resolves the drop slot at the given viewport position, hitting either a
    // real track or the virtual append slot. Returns nullopt outside the
    // canvas (e.g. over the timeline ruler).
    [[nodiscard]] std::optional<TrackDropSlot> dropSlotAt(const QPointF &viewportPosition) const;
    void updateExternalDropOverlay(const QPointF &viewportPosition);
    void endExternalDropOverlay();
    void updateExternalDropScrollState(const QPointF &viewportPosition);
    void onExternalDropScrollFrame(double dtMs);

    QColor backgroundColor() const;
    void setBackgroundColor(const QColor &color);
    QColor barLineColor() const;
    void setBarLineColor(const QColor &color);
    QColor beatLineColor() const;
    void setBeatLineColor(const QColor &color);
    QColor commonLineColor() const;
    void setCommonLineColor(const QColor &color);
    QColor playPosIndicatorColor() const;
    void setPlayPosIndicatorColor(const QColor &color);
    QColor lastPlayPosIndicatorColor() const;
    void setLastPlayPosIndicatorColor(const QColor &color);
    QColor selectedTrackColor() const;
    void setSelectedTrackColor(const QColor &color);
    QColor clipSelectedBorderColor() const;
    void setClipSelectedBorderColor(const QColor &color);
    QColor rubberBandBorderColor() const;
    void setRubberBandBorderColor(const QColor &color);
    QColor rubberBandFillColor() const;
    void setRubberBandFillColor(const QColor &color);
    QColor dropHighlightColor() const;
    void setDropHighlightColor(const QColor &color);
    QColor dropIndicatorColor() const;
    void setDropIndicatorColor(const QColor &color);

    EditorViewportController m_viewport;
    std::unique_ptr<EditorWheelController> m_wheelController;
    EditorTouchController *m_touchController = nullptr;
    EditorPenController *m_penController = nullptr;
    // Last known pointer position in widget coordinates. Timer-driven auto
    // scroll must read this instead of QCursor::pos(), which does not move
    // with a finger.
    QPointF m_lastPointerPosition;
    EditorGlyphAtlas m_glyphAtlas;
    EditorRhiScrollBarController *m_scrollBars = nullptr;
    // Shared inline rename session for clip titles (see ClipTitleInlineEdit).
    ClipTitleInlineEdit *m_titleEditor = nullptr;
    int m_titleEditingClipId = -1;
    // Viewport offset when the title editor was anchored. Used to tell a real
    // content move from a host resize, which must keep the editor.
    QPointF m_titleEditOffset;
    QHash<int, std::shared_ptr<AudioWaveformSampler>> m_audioWaveformSamplers;
    QVector<ClipSnapshot> m_clipSnapshots;
    QVector<ClipSnapshot> m_pastePreviewSnapshots;
    QVector<EditorRhiOverlayRect> m_playbackOverlayRects;
    bool m_snapshotScheduled = false;
    // rebuildModelConnections() runs on every model change; this keeps the controller connection
    // from stacking up across those rebuilds.
    bool m_loadStateConnected = false;
    // Rebuilds the frame while a visible clip is still loading. The loading ring turns by clock and
    // the snapshots are rebuilt each frame, so nothing would move without this ticker.
    QTimer m_hintTicker;
    double m_playbackPosition = 0.0;
    double m_lastPlaybackPosition = 0.0;
    bool m_autoTurnPage = true;
    bool m_autoPageTurnAvailable = false;
    double m_leftMarginPx = 0.0;
    int m_baseSceneLength = 0;
    int m_sceneLengthExtension = 0;
    DragMode m_dragMode = DragMode::None;
    QPointF m_rubberBandStart;
    QPointF m_rubberBandEnd;
    // Clip selection and the gesture in flight. The drag math and the per-clip targets live in the
    // shared planner, so this backend only owns the preview map and the commit plumbing.
    EditorSelectionUtils::OrderedSelectionModel m_selectionModel;
    ClipSelectionDrag::Session m_dragSession;
    QHash<int, ClipSelectionDrag::Placement> m_placements;
    int m_dragAnchorId = -1;
    int m_dragTrackIndex = -1;
    bool m_dragMoved = false;
    quint64 m_dragSessionId = 0;

    // External file drag-and-drop state (Phase 1)
    bool m_externalDragActive = false;
    std::optional<TrackDropSlot> m_dropSlot;
    EdgeAutoScroller m_edgeAutoScroller;
    QPointF m_dropDragStartPos;
    bool m_dropScrollDistanceReached = false;

    QColor m_backgroundColor{30, 32, 36};
    QColor m_barLineColor{8, 9, 10};
    QColor m_beatLineColor{22, 25, 28};
    QColor m_commonLineColor{28, 32, 36};
    QColor m_playPosIndicatorColor{200, 200, 200};
    QColor m_lastPlayPosIndicatorColor{160, 160, 160};
    QColor m_selectedTrackColor{0x31, 0x35, 0x3F};
    QColor m_clipSelectedBorderColor{255, 255, 255};
    QColor m_rubberBandBorderColor{155, 186, 255, 200};
    QColor m_rubberBandFillColor{155, 186, 255, 64};
    // External drop overlay colors (theme-injected via QSS)
    QColor m_dropHighlightColor{0xA9, 0xC4, 0xFF, 0x50};
    QColor m_dropIndicatorColor{200, 200, 200};

    // "Split Clip" context menu hover preview state
    bool m_splitPreviewActive = false;
    int m_splitPreviewTrackIndex = -1;
    int m_splitPreviewTick = 0;
};

#endif // TRACKSRHIWIDGET_H
