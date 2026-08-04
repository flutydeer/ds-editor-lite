#ifndef TRACKSRHIWIDGET_H
#define TRACKSRHIWIDGET_H

#include "Interface/EditorViewState.h"
#include "TrackEditorContextMenuController.h"
#include "UI/Views/Common/EditorGlyphAtlas.h"
#include "UI/Views/Common/EditorRhiWidget.h"
#include "UI/Views/Common/EditorViewportController.h"

#include <lite/History/HistoryFocus.h>
#include <lite/ProjectModel/AppModel/Clip.h>

#include <QTimer>

#include <optional>

class QContextMenuEvent;
class QKeyEvent;
class QHideEvent;
class QMouseEvent;
class QResizeEvent;
class QShowEvent;
class QWheelEvent;

class TracksRhiWidget final : public EditorRhiWidget, public ITrackPastePreviewHost {
    Q_OBJECT
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
    void showTrackPastePreview(const TrackPastePreviewData &data, int previewTick,
                               int baseTrackIndex) override;
    void clearTrackPastePreview() override;
    // WYSIWYG snap step at `tick` for the current zoom level.
    [[nodiscard]] int snapStep(int tick) const;

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

protected:
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
    void leaveEvent(QEvent *event) override;
    void onRhiReady() override;
    void onDevicePixelRatioChanged() override;

private:
    enum class DragMode { None, Move, ResizeLeft, ResizeRight, RectSelect };

    struct NoteSnapshot {
        int start = 0;
        int length = 0;
        int key = 60;
    };

    struct ClipSnapshot {
        int id = -1;
        int trackIndex = -1;
        int colorIndex = 0;
        IClip::ClipType type = IClip::Generic;
        int contentStartTick = 0;
        int visibleStartTick = 0;
        int visibleEndTick = 0;
        QRectF physicalRect;
        QString title;
        bool selected = false;
        bool active = false;
        bool pastePreview = false;
        QVector<NoteSnapshot> notes;
        QVector<std::tuple<short, short>> peaks;
    };

    struct DragPreview {
        int clipId = -1;
        int trackIndex = -1;
        Clip::ClipCommonProperties properties;
    };

    void rebuildSnapshot();
    void rebuildModelConnections();
    void appendGrid(EditorRhiFrameData &frame, double dpr) const;
    void appendClips(EditorRhiFrameData &frame, double dpr);
    void appendClip(EditorRhiFrameData &frame, const ClipSnapshot &clip, double dpr);
    void appendPlaybackIndicators(EditorRhiFrameData &frame, double dpr) const;
    [[nodiscard]] ClipSnapshot buildClipSnapshot(const Clip *clip, int trackIndex,
                                                 double dpr) const;
    [[nodiscard]] const ClipSnapshot *hitTest(const QPointF &viewportPosition) const;
    [[nodiscard]] double wheelDelta(const QWheelEvent *event, bool preferHorizontal) const;
    [[nodiscard]] int trackIndexAt(const QPointF &viewportPosition) const;
    [[nodiscard]] int tickAt(const QPointF &viewportPosition) const;
    [[nodiscard]] int snapTick(int tick) const;
    void beginClipDrag(const ClipSnapshot &clip, const QMouseEvent *event);
    void updateDrag(const QPointF &position, Qt::KeyboardModifiers modifiers);
    void commitDrag();
    void discardDrag();
    void syncSelection(const QList<int> &ids, int preferredTrack = -1) const;
    void updateCursor(const QPointF &position);
    void handleAutoPageTurn();
    void updateAutoPageTurnAvailability();
    [[nodiscard]] Clip::ClipCommonProperties previewOrModelProperties(const Clip *clip) const;

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

    EditorViewportController m_viewport;
    EditorGlyphAtlas m_glyphAtlas;
    QVector<ClipSnapshot> m_clipSnapshots;
    QVector<ClipSnapshot> m_pastePreviewSnapshots;
    bool m_snapshotScheduled = false;
    double m_playbackPosition = 0.0;
    double m_pendingPlaybackPosition = 0.0;
    double m_lastPlaybackPosition = 0.0;
    QTimer m_positionThrottle;
    bool m_autoTurnPage = true;
    bool m_autoPageTurnAvailable = false;
    DragMode m_dragMode = DragMode::None;
    QPointF m_mouseDownScene;
    QPointF m_rubberBandStart;
    QPointF m_rubberBandEnd;
    std::optional<DragPreview> m_dragPreview;
    Clip::ClipCommonProperties m_mouseDownProperties;
    int m_mouseDownTrackIndex = -1;
    bool m_dragMoved = false;

    QColor m_barLineColor{8, 9, 10};
    QColor m_beatLineColor{22, 25, 28};
    QColor m_commonLineColor{28, 32, 36};
    QColor m_playPosIndicatorColor{200, 200, 200};
    QColor m_lastPlayPosIndicatorColor{160, 160, 160};
    QColor m_selectedTrackColor{0x31, 0x35, 0x3F};
    QColor m_clipSelectedBorderColor{255, 255, 255};
};

#endif // TRACKSRHIWIDGET_H
