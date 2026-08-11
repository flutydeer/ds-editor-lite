#include "TracksRhiWidget.h"

#include "ClipResizeUtils.h"
#include "SingingClipPreviewLayout.h"
#include "Controller/EditorViewController.h"
#include "Controller/PlaybackController.h"
#include "Controller/TrackController.h"
#include "Global/AppGlobal.h"
#include "Global/TracksEditorGlobal.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Utils/AppColorPalette.h"
#include "UI/Utils/ITimelinePainter.h"
#include "UI/Utils/SpeakerMixDisplayUtils.h"
#include "UI/Views/Common/AutoPageTurnUtils.h"
#include "UI/Views/Common/EditorRhiScrollBarController.h"

#include <lite/MusicBase/TimelineSnapUtils.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QApplication>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFontMetricsF>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLocale>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QSet>
#include <QShowEvent>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <functional>

using namespace TracksEditorGlobal;

namespace {
    constexpr double kRasterLineOpacity = 0.25;

    QColor withOpacity(const QColor &source, const double opacity) {
        auto color = source;
        color.setAlphaF(color.alphaF() * opacity);
        return color;
    }

    QString commonClipTitle(const Clip::ClipCommonProperties &properties, const int id,
                            const double scaleX, const double scaleY) {
        const auto showDebug = appOptions->developer()->showClipDebugInfo;
        const auto control = (showDebug ? QStringLiteral("id: %1 ").arg(id) : QString()) +
                             QStringLiteral("%1 %2dB %3 ")
                                 .arg(properties.name, QLocale().toString(properties.gain),
                                      properties.mute ? QStringLiteral("M") : QString());
        if (!showDebug)
            return control;
        return control + QStringLiteral("s: %1 l: %2 cs: %3 cl: %4 sx: %5 sy: %6")
                             .arg(properties.start)
                             .arg(properties.length)
                             .arg(properties.clipStart)
                             .arg(properties.clipLen)
                             .arg(scaleX)
                             .arg(scaleY);
    }

    QColor blendColor(const QColor &from, const QColor &to, double ratio) {
        ratio = std::clamp(ratio, 0.0, 1.0);
        return QColor(static_cast<int>(from.red() + (to.red() - from.red()) * ratio),
                      static_cast<int>(from.green() + (to.green() - from.green()) * ratio),
                      static_cast<int>(from.blue() + (to.blue() - from.blue()) * ratio));
    }

    class TimelineEmitter final : public ITimelinePainter {
    public:
        using Callback = std::function<void(int, const QColor &)>;

        void emitLines(const Timeline &timeline, const int quantize, const double start,
                       const double end, const double width, const QColor &bar, const QColor &beat,
                       const QColor &common, Callback callback) {
            setTimeline(timeline);
            setQuantize(quantize);
            m_bar = bar;
            m_beat = beat;
            m_common = common;
            m_callback = std::move(callback);
            QImage image(1, 1, QImage::Format_ARGB32_Premultiplied);
            QPainter painter(&image);
            drawTimeline(&painter, start, end, width);
        }

        int gridStep(const Timeline &timeline, const int quantize, const double ticksPerPixel,
                     const int atTick) {
            setTimeline(timeline);
            setQuantize(quantize);
            return logicalGridStepForScale(ticksPerPixel, atTick);
        }

    private:
        QColor withPainterOpacity(const QColor &source, const QPainter *painter) const {
            auto color = source;
            color.setAlphaF(color.alphaF() * painter->opacity());
            return color;
        }

        void drawBar(QPainter *painter, const int tick, int) override {
            m_callback(tick, withPainterOpacity(m_bar, painter));
        }

        void drawBeat(QPainter *painter, const int tick, int, int) override {
            m_callback(tick, withPainterOpacity(m_beat, painter));
        }

        void drawSubdivision(QPainter *painter, const int tick, const int level,
                             const int levelCount) override {
            const auto ratio = levelCount > 1 ? static_cast<double>(level) / (levelCount - 1) : 0.0;
            m_callback(tick, withPainterOpacity(blendColor(m_beat, m_common, ratio), painter));
        }

        QColor m_bar;
        QColor m_beat;
        QColor m_common;
        Callback m_callback;
    };

}

TracksRhiWidget::TracksRhiWidget(QWidget *parent)
    : EditorRhiWidget(QStringLiteral("TracksRhi"), parent), m_viewport(this),
      m_glyphAtlas(QSize(1024, 1024), 4) {
    setObjectName(QStringLiteral("TracksRhiWidget"));
    setMouseTracking(true);
    setAcceptDrops(true);
    m_viewport.setPixelsPerQuarterNote(pixelsPerQuarterNote);
    m_viewport.setScaleBounds(0.0001, 10000.0, 0.575, 8.0);
    m_viewport.setEnsureContentFillsViewport(true, false);
    m_viewport.setContentTickRange(0.0, appStatus->projectEditableLength);
    // One extra unit for the virtual append slot at the bottom of the canvas
    m_viewport.setVerticalContent(appModel->tracks().size() + 1, trackHeight);

    m_scrollBars = new EditorRhiScrollBarController(this, this);
    connect(m_scrollBars, &EditorRhiScrollBarController::offsetChangeRequested, this,
            [this](const QPointF &offset) {
                m_viewport.scrollBy({offset.x() - m_viewport.horizontalOffset(),
                                     offset.y() - m_viewport.verticalOffset()});
            });

    connect(&m_viewport, &EditorViewportController::scaleChanged, this,
            &TracksRhiWidget::scaleChanged);
    connect(&m_viewport, &EditorViewportController::timeRangeChanged, this,
            &TracksRhiWidget::timeRangeChanged);
    connect(&m_viewport, &EditorViewportController::timeRangeChanged, this,
            &TracksRhiWidget::updateAutoPageTurnAvailability);
    connect(&m_viewport, &EditorViewportController::scrollChanged, this,
            [this](const double, const double vertical) {
                emit verticalOffsetChanged(vertical);
                emit visibleRectChanged(logicalVisibleRect());
            });
    connect(&m_viewport, &EditorViewportController::viewportChanged, this, [this] {
        updateScrollBars();
        scheduleSnapshot();
    });
    connect(&m_edgeAutoScroller, &EdgeAutoScroller::frame, this,
            &TracksRhiWidget::onExternalDropScrollFrame);

    connect(appModel, &AppModel::modelChanged, this, [this] {
        rebuildModelConnections();
        m_viewport.setVerticalContent(appModel->tracks().size() + 1, trackHeight);
        scheduleSnapshot();
    });
    connect(appModel, &AppModel::trackChanged, this, [this] {
        rebuildModelConnections();
        m_viewport.setVerticalContent(appModel->tracks().size() + 1, trackHeight);
        scheduleSnapshot();
    });
    connect(appModel, &AppModel::trackMoved, this, [this] {
        rebuildModelConnections();
        scheduleSnapshot();
    });
    connect(appModel, &AppModel::timelineChanged, this, [this] {
        for (const auto &sampler : m_audioWaveformSamplers)
            sampler->invalidate();
        updateAutoPageTurnAvailability();
        scheduleSnapshot();
    });
    connect(appStatus, &AppStatus::selectedTrackIndexChanged, this,
            &TracksRhiWidget::scheduleSnapshot);
    connect(appStatus, &AppStatus::clipSelectionChanged, this, &TracksRhiWidget::scheduleSnapshot);
    connect(appStatus, &AppStatus::activeClipIdChanged, this, &TracksRhiWidget::scheduleSnapshot);
    connect(appStatus, &AppStatus::pianoRollVisibleRectChanged, this,
            &TracksRhiWidget::scheduleSnapshot);
    connect(appStatus, &AppStatus::projectEditableLengthChanged, this,
            &TracksRhiWidget::setSceneLength);
    connect(appOptions, &AppOptions::optionsChanged, this,
            [this](const AppOptionsGlobal::Option option) {
                if (option == AppOptionsGlobal::DeveloperOptions ||
                    option == AppOptionsGlobal::All) {
                    scheduleSnapshot();
                }
            });
    m_positionThrottle.setSingleShot(true);
    m_positionThrottle.setInterval(33);
    connect(&m_positionThrottle, &QTimer::timeout, this, [this] {
        m_playbackPosition = m_pendingPlaybackPosition;
        handleAutoPageTurn();
        scheduleSnapshot();
    });
    rebuildModelConnections();
    QTimer::singleShot(0, this, [this] {
        updateScrollBars();
        updateAutoPageTurnAvailability();
    });
}

TracksRhiWidget::~TracksRhiWidget() = default;

TrackPanelViewState TracksRhiWidget::viewState() const {
    const auto state = m_viewport.state();
    return {state.centerTick, state.centerUnit - 0.5, state.horizontalScale, state.verticalScale};
}

bool TracksRhiWidget::centerAt(const double tick, const double trackIndex) {
    return m_viewport.centerAt(tick, trackIndex + 0.5);
}

bool TracksRhiWidget::setViewScale(const double horizontalScale, const double verticalScale) {
    const auto state = viewState();
    if (!m_viewport.setScale(horizontalScale, verticalScale,
                             QPointF(width() * 0.5, height() * 0.5))) {
        return false;
    }
    return centerAt(state.centerTick, state.centerTrackIndex);
}

HistoryFocusVisibility TracksRhiWidget::focusVisibility(const HistoryFocus &focus) const {
    if (!focus.isValid() || focus.kind != HistoryFocusKind::TrackClips)
        return HistoryFocusVisibility::Unavailable;
    QRectF focusRect;
    for (const auto id : focus.objectIds) {
        int trackIndex = -1;
        if (const auto *clip = appModel->findClipById(id, trackIndex)) {
            const auto left = m_viewport.tickToSceneX(clip->start() + clip->clipStart());
            const auto right =
                m_viewport.tickToSceneX(clip->start() + clip->clipStart() + clip->clipLen());
            const QRectF rect(left, m_viewport.unitToSceneY(trackIndex),
                              std::max(1.0, right - left), trackHeight * scaleY());
            focusRect = focusRect.isNull() ? rect : focusRect.united(rect);
        }
    }
    int trackIndex = focus.trackIndex;
    if (focus.trackId >= 0)
        appModel->findTrackById(focus.trackId, trackIndex);
    if (focusRect.isNull() && trackIndex >= 0 && trackIndex < appModel->tracks().size()) {
        focusRect =
            QRectF(m_viewport.tickToSceneX(focus.tickStart), m_viewport.unitToSceneY(trackIndex),
                   std::max(1.0, m_viewport.tickToSceneX(focus.tickEnd) -
                                     m_viewport.tickToSceneX(focus.tickStart)),
                   trackHeight * scaleY());
    }
    if (focusRect.isNull())
        return HistoryFocusVisibility::ContextSwitchRequired;
    return m_viewport.visibleSceneRect().intersects(focusRect)
               ? HistoryFocusVisibility::Visible
               : HistoryFocusVisibility::ScrollRequired;
}

bool TracksRhiWidget::revealFocus(const HistoryFocus &focus, bool) {
    if (!focus.isValid() || focus.kind != HistoryFocusKind::TrackClips)
        return false;
    QList<int> selectedIds;
    QRectF bounds;
    for (const auto id : focus.objectIds) {
        int trackIndex = -1;
        if (const auto *clip = appModel->findClipById(id, trackIndex)) {
            selectedIds.append(id);
            const auto left = m_viewport.tickToSceneX(clip->start() + clip->clipStart());
            const auto right =
                m_viewport.tickToSceneX(clip->start() + clip->clipStart() + clip->clipLen());
            const QRectF rect(left, m_viewport.unitToSceneY(trackIndex),
                              std::max(1.0, right - left), trackHeight * scaleY());
            bounds = bounds.isNull() ? rect : bounds.united(rect);
        }
    }
    if (!selectedIds.isEmpty()) {
        syncSelection(selectedIds);
        trackController->setActiveClip(selectedIds.first());
        const auto centerTick = m_viewport.sceneXToTick(bounds.center().x());
        const auto centerTrack = bounds.center().y() / (trackHeight * scaleY()) - 0.5;
        return centerAt(centerTick, centerTrack);
    }
    int trackIndex = focus.trackIndex;
    if (focus.trackId >= 0)
        appModel->findTrackById(focus.trackId, trackIndex);
    if (trackIndex < 0)
        trackIndex = qRound((focus.valueStart + focus.valueEnd) * 0.5);
    if (trackIndex < 0)
        return false;
    return centerAt((focus.tickStart + focus.tickEnd) * 0.5, trackIndex);
}

QRectF TracksRhiWidget::logicalVisibleRect() const {
    return m_viewport.visibleSceneRect();
}

double TracksRhiWidget::scaleX() const {
    return m_viewport.horizontalScale();
}

double TracksRhiWidget::scaleY() const {
    return m_viewport.verticalScale();
}

double TracksRhiWidget::startTick() const {
    return m_viewport.startTick();
}

double TracksRhiWidget::endTick() const {
    return m_viewport.endTick();
}

void TracksRhiWidget::setSceneLength(const int tick) {
    m_viewport.setContentTickRange(0.0, std::max(0, tick));
    updateAutoPageTurnAvailability();
}

void TracksRhiWidget::setLeftMarginPx(const double px) {
    m_leftMarginPx = px;
    m_viewport.setLeftMarginPx(px);
}

void TracksRhiWidget::setPlaybackPosition(const double tick) {
    m_pendingPlaybackPosition = tick;
    if (!m_positionThrottle.isActive())
        m_positionThrottle.start();
}

void TracksRhiWidget::setLastPlaybackPosition(const double tick) {
    m_lastPlaybackPosition = tick;
    scheduleSnapshot();
}

void TracksRhiWidget::onWheelHorScale(QWheelEvent *event) {
    m_viewport.zoomHorizontal(wheelDelta(event, false), event->position().x());
}

void TracksRhiWidget::onWheelVerScale(QWheelEvent *event) {
    m_viewport.zoomVertical(wheelDelta(event, true), event->position().y());
}

void TracksRhiWidget::onWheelHorScroll(QWheelEvent *event) {
    m_viewport.scrollBy({-width() * 0.2 * wheelDelta(event, false) / 120.0, 0.0});
}

void TracksRhiWidget::onWheelVerScroll(QWheelEvent *event) {
    m_viewport.scrollBy({0.0, -height() * 0.15 * wheelDelta(event, false) / 120.0});
}

void TracksRhiWidget::setVerticalOffset(const double value) {
    m_viewport.scrollBy({0.0, value - m_viewport.verticalOffset()});
}

void TracksRhiWidget::updateScrollBars() {
    if (!m_scrollBars)
        return;
    m_scrollBars->setMetrics(QSizeF(m_viewport.tickToSceneX(appStatus->projectEditableLength),
                                    m_viewport.unitToSceneY(appModel->tracks().size() + 1)),
                             QPointF(m_viewport.horizontalOffset(), m_viewport.verticalOffset()),
                             QSizeF(std::max(1, width() / 10),
                                    std::max(1, qRound(trackHeight * m_viewport.verticalScale()))));
}

void TracksRhiWidget::scheduleSnapshot() {
    if (m_snapshotScheduled)
        return;
    m_snapshotScheduled = true;
    QTimer::singleShot(0, this, [this] {
        m_snapshotScheduled = false;
        rebuildSnapshot();
    });
}

void TracksRhiWidget::resizeEvent(QResizeEvent *event) {
    EditorRhiWidget::resizeEvent(event);
    m_viewport.setViewportSize(event->size());
    emit sizeChanged(event->size());
}

void TracksRhiWidget::showEvent(QShowEvent *event) {
    EditorRhiWidget::showEvent(event);
    updateAutoPageTurnAvailability();
}

void TracksRhiWidget::hideEvent(QHideEvent *event) {
    EditorRhiWidget::hideEvent(event);
    updateAutoPageTurnAvailability();
}

void TracksRhiWidget::wheelEvent(QWheelEvent *event) {
    if (event->modifiers() == Qt::ControlModifier)
        onWheelHorScale(event);
    else if (event->modifiers() == Qt::AltModifier)
        onWheelVerScale(event);
    else if (event->modifiers() == Qt::ShiftModifier)
        onWheelHorScroll(event);
    else
        onWheelVerScroll(event);
    event->accept();
}

void TracksRhiWidget::mousePressEvent(QMouseEvent *event) {
    setFocus(Qt::MouseFocusReason);
    if (event->button() != Qt::LeftButton) {
        EditorRhiWidget::mousePressEvent(event);
        return;
    }
    const auto *hit = hitTest(event->position());
    if (hit) {
        auto selected = appStatus->selectedClips.get();
        if (event->modifiers() == Qt::ControlModifier) {
            if (selected.contains(hit->id))
                selected.removeAll(hit->id);
            else
                selected.append(hit->id);
        } else if (!selected.contains(hit->id)) {
            selected = {hit->id};
        }
        syncSelection(selected, hit->trackIndex);
        if (selected.contains(hit->id)) {
            trackController->setActiveClip(hit->id);
            beginClipDrag(*hit, event);
        }
    } else {
        syncSelection({});
        m_dragMode = DragMode::RectSelect;
        m_rubberBandStart = m_viewport.viewportToScene(event->position());
        m_rubberBandEnd = m_rubberBandStart;
    }
    scheduleSnapshot();
    event->accept();
}

void TracksRhiWidget::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragMode == DragMode::RectSelect) {
        m_rubberBandEnd = m_viewport.viewportToScene(event->position());
        const auto dpr = devicePixelRatioF();
        const QRectF selection(m_rubberBandStart * dpr, m_rubberBandEnd * dpr);
        QList<int> ids;
        for (const auto &clip : m_clipSnapshots)
            if (selection.normalized().intersects(clip.physicalRect))
                ids.append(clip.id);
        syncSelection(ids);
        scheduleSnapshot();
    } else if (m_dragMode != DragMode::None) {
        updateDrag(event->position(), event->modifiers());
    } else {
        updateCursor(event->position());
    }
    event->accept();
}

void TracksRhiWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        if (m_dragMode == DragMode::RectSelect) {
            m_dragMode = DragMode::None;
        } else if (m_dragMode != DragMode::None) {
            commitDrag();
        }
        scheduleSnapshot();
    }
    event->accept();
}

void TracksRhiWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    if (const auto *hit = hitTest(event->position())) {
        trackController->setActiveClip(hit->id);
        editorViewController->showBottomPanelPage(QStringLiteral("ClipEditor"));
        double targetTick = playbackController->position();
        double targetKey = 60.0;
        if (hit->type == IClip::Singing && !hit->notes.isEmpty()) {
            const auto dpr = devicePixelRatioF();
            const auto physicalPosition = m_viewport.viewportToScene(event->position()) * dpr;
            const auto preview = clipPreviewRect(*hit, dpr);
            QList<int> keys;
            keys.reserve(hit->notes.size());
            for (const auto &note : hit->notes)
                keys.append(note.key);
            const auto layout = SingingClipPreview::computeLayout(
                preview, keys, SingingClipPreview::maximumNoteHeight * dpr);
            if (preview.contains(physicalPosition) && layout.valid()) {
                targetTick = tickAt(event->position());
                targetKey = layout.keyIndexAt(physicalPosition.y());
            }
        }
        editorViewController->centerPianoRollAt(targetTick, targetKey);
    } else {
        const auto trackIndex = trackIndexAt(event->position());
        if (trackIndex >= 0) {
            const auto tick = tickAt(event->position());
            trackController->onNewSingingClip(
                trackIndex,
                TimelineSnapUtils::snapDown(tick, snapStep(tick), appModel->timeline()));
        }
    }
    event->accept();
}

void TracksRhiWidget::contextMenuEvent(QContextMenuEvent *event) {
    TrackEditorMenuContext context;
    context.globalPos = event->globalPos();
    context.rawTick = tickAt(event->pos());
    context.snappedTick = TimelineSnapUtils::snapDown(context.rawTick, snapStep(context.rawTick),
                                                      appModel->timeline());
    if (const auto *hit = hitTest(event->pos())) {
        if (!appStatus->selectedClips.get().contains(hit->id))
            syncSelection({hit->id}, hit->trackIndex);
        context.trackIndex = hit->trackIndex;
        context.clipId = hit->id;
        context.selectedClipIds = appStatus->selectedClips.get();
        context.target = hit->type == IClip::Audio ? TrackEditorMenuContext::Target::AudioClip
                                                   : TrackEditorMenuContext::Target::SingingClip;
        if (const auto *audio = qobject_cast<const AudioClip *>(appModel->findClipById(hit->id)))
            context.audioMissing = audio->pathStatus() == AudioClip::PathStatus::Missing;
    } else {
        context.trackIndex = trackIndexAt(event->pos());
        if (context.trackIndex < 0)
            return;
        context.target = TrackEditorMenuContext::Target::Background;
    }
    emit contextMenuRequested(context);
    event->accept();
}

void TracksRhiWidget::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        m_externalDragActive = true;
        m_dropScrollDistanceReached = false;
        m_dropDragStartPos = event->position();
        updateExternalDropOverlay(event->position());
        return;
    }
    QWidget::dragEnterEvent(event);
}

void TracksRhiWidget::dragMoveEvent(QDragMoveEvent *event) {
    if (m_externalDragActive && event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        updateExternalDropOverlay(event->position());
        updateExternalDropScrollState(event->position());
        return;
    }
    QWidget::dragMoveEvent(event);
}

void TracksRhiWidget::dragLeaveEvent(QDragLeaveEvent *event) {
    if (m_externalDragActive) {
        endExternalDropOverlay();
        event->accept();
        return;
    }
    QWidget::dragLeaveEvent(event);
}

void TracksRhiWidget::dropEvent(QDropEvent *event) {
    if (m_externalDragActive && event->mimeData()->hasUrls()) {
        if (const auto slot = dropSlotAt(event->position()))
            emit externalDropRequested(*slot, event->mimeData()->urls());
        endExternalDropOverlay();
        event->acceptProposedAction();
        return;
    }
    QWidget::dropEvent(event);
}

std::optional<TrackDropSlot> TracksRhiWidget::dropSlotAt(const QPointF &viewportPosition) const {
    const auto scenePos = m_viewport.viewportToScene(viewportPosition);
    if (scenePos.y() < 0)
        return std::nullopt;
    const auto unit = m_viewport.sceneYToUnit(scenePos.y());
    const auto trackIndex = static_cast<int>(std::floor(unit));
    TrackDropSlot slot;
    slot.snappedTick = snapTick(tickAt(viewportPosition));
    if (trackIndex >= 0 && trackIndex < appModel->tracks().size()) {
        slot.kind = TrackDropSlot::Kind::ExistingTrack;
        slot.trackIndex = trackIndex;
        return slot;
    }
    if (unit >= appModel->tracks().size() && unit < appModel->tracks().size() + 1.0) {
        slot.kind = TrackDropSlot::Kind::Append;
        slot.trackIndex = appModel->tracks().size();
        return slot;
    }
    return std::nullopt;
}

void TracksRhiWidget::updateExternalDropOverlay(const QPointF &viewportPosition) {
    m_dropSlot = dropSlotAt(viewportPosition);
    scheduleSnapshot();
}

void TracksRhiWidget::endExternalDropOverlay() {
    m_externalDragActive = false;
    m_dropSlot.reset();
    m_dropScrollDistanceReached = false;
    m_edgeAutoScroller.stop();
    scheduleSnapshot();
}

void TracksRhiWidget::updateExternalDropScrollState(const QPointF &viewportPosition) {
    if (!m_externalDragActive)
        return;
    if (!m_dropScrollDistanceReached) {
        if ((viewportPosition - m_dropDragStartPos).manhattanLength() <
            QApplication::startDragDistance())
            return;
        m_dropScrollDistanceReached = true;
    }
    const QRectF viewportRect(QPointF(0, 0), size());
    const auto velocity = EdgeAutoScroller::velocity(viewportPosition, viewportRect, Qt::Vertical,
                                                     m_edgeAutoScroller.config());
    const bool inHotZone = !velocity.isNull();
    if (inHotZone && !m_edgeAutoScroller.isRunning())
        m_edgeAutoScroller.start();
    else if (!inHotZone && m_edgeAutoScroller.isRunning())
        m_edgeAutoScroller.stop();
}

void TracksRhiWidget::onExternalDropScrollFrame(const double dtMs) {
    // Safety net: stop if the drag ended without us seeing the event
    if (!m_externalDragActive || QGuiApplication::mouseButtons() == Qt::NoButton || !isVisible()) {
        m_edgeAutoScroller.stop();
        return;
    }
    const auto pointerPos = QPointF(mapFromGlobal(QCursor::pos()));
    const QRectF viewportRect(QPointF(0, 0), size());
    const auto step = m_edgeAutoScroller.computeStep(pointerPos, viewportRect, Qt::Vertical, dtMs);
    if (step.y() != 0)
        m_viewport.scrollBy({0.0, static_cast<double>(step.y())});
    const auto clamped = EdgeAutoScroller::clampToRect(pointerPos, viewportRect);
    updateExternalDropOverlay(clamped);
    // Stop the timer once the pointer left the hot zone (it may re-enter later)
    updateExternalDropScrollState(pointerPos);
}

void TracksRhiWidget::appendDropOverlay(EditorRhiFrameData &frame, const double dpr) const {
    if (!m_externalDragActive || !m_dropSlot)
        return;
    const auto slot = *m_dropSlot;
    const auto sceneWidth = m_viewport.tickToSceneX(appStatus->projectEditableLength);
    const auto trackHeightPx = trackHeight * scaleY();
    const auto top = slot.trackIndex * trackHeightPx;
    EditorRhiGeometry::appendRect(frame.solidVertices,
                                  QRectF(0, top * dpr, sceneWidth * dpr, trackHeightPx * dpr),
                                  m_dropHighlightColor);
    EditorRhiGeometry::appendPixelAlignedHorizontalLine(frame.solidVertices, top * dpr, 0.0,
                                                        sceneWidth * dpr, m_dropIndicatorColor);
    EditorRhiGeometry::appendPixelAlignedHorizontalLine(frame.solidVertices,
                                                        (top + trackHeightPx) * dpr, 0.0,
                                                        sceneWidth * dpr, m_dropIndicatorColor);
    const auto contentHeight = (appModel->tracks().size() + 1) * trackHeightPx * dpr;
    EditorRhiGeometry::appendPixelAlignedVerticalLine(
        frame.solidVertices, m_viewport.tickToSceneX(slot.snappedTick) * dpr, 0.0, contentHeight,
        m_dropIndicatorColor);
}

QColor TracksRhiWidget::dropHighlightColor() const {
    return m_dropHighlightColor;
}

void TracksRhiWidget::setDropHighlightColor(const QColor &color) {
    if (m_dropHighlightColor == color)
        return;
    m_dropHighlightColor = color;
    scheduleSnapshot();
}

QColor TracksRhiWidget::dropIndicatorColor() const {
    return m_dropIndicatorColor;
}

void TracksRhiWidget::setDropIndicatorColor(const QColor &color) {
    if (m_dropIndicatorColor == color)
        return;
    m_dropIndicatorColor = color;
    scheduleSnapshot();
}

void TracksRhiWidget::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        discardDrag();
        event->accept();
        return;
    }
    EditorRhiWidget::keyPressEvent(event);
}

void TracksRhiWidget::leaveEvent(QEvent *event) {
    unsetCursor();
    EditorRhiWidget::leaveEvent(event);
}

void TracksRhiWidget::onRhiReady() {
    scheduleSnapshot();
}

void TracksRhiWidget::onDevicePixelRatioChanged() {
    EditorRhiWidget::onDevicePixelRatioChanged();
    m_glyphAtlas.clear();
    scheduleSnapshot();
}

void TracksRhiWidget::rebuildSnapshot() {
    const auto dpr = devicePixelRatioF();
    EditorRhiFrameData frame;
    frame.clearColor = m_backgroundColor;
    frame.physicalCameraOffset =
        QPointF(m_viewport.horizontalOffset(), m_viewport.verticalOffset()) * dpr;
    m_glyphAtlas.beginFrame();
    appendGrid(frame, dpr);
    appendClips(frame, dpr);
    appendPlaybackIndicators(frame, dpr);
    appendDropOverlay(frame, dpr);
    if (m_dragMode == DragMode::RectSelect) {
        const auto rect = QRectF(m_rubberBandStart * dpr, m_rubberBandEnd * dpr).normalized();
        const auto radius = std::min({6.0 * dpr, rect.width() * 0.5, rect.height() * 0.5});
        EditorRhiGeometry::appendRoundedRect(frame.solidVertices, rect, radius,
                                             m_rubberBandFillColor);
        EditorRhiGeometry::appendRoundedRectStroke(frame.solidVertices, rect, radius, 1.5 * dpr,
                                                   m_rubberBandBorderColor);
    }
    frame.textureBatches = m_glyphAtlas.textureBatches();
    submitFrame(std::move(frame));
}

void TracksRhiWidget::rebuildModelConnections() {
    for (const auto *track : appModel->tracks()) {
        disconnect(track, nullptr, this, nullptr);
        connect(track, &Track::propertyChanged, this, &TracksRhiWidget::scheduleSnapshot);
        connect(track, &Track::clipChanged, this, [this](Track::ClipChangeType, Clip *) {
            rebuildModelConnections();
            scheduleSnapshot();
        });
        for (const auto *clip : track->clips()) {
            disconnect(clip, nullptr, this, nullptr);
            connect(clip, &Clip::propertyChanged, this, [this, clip] {
                if (const auto sampler = m_audioWaveformSamplers.value(clip->id()))
                    sampler->invalidate();
                scheduleSnapshot();
            });
            if (const auto *singing = qobject_cast<const SingingClip *>(clip)) {
                connect(singing, &SingingClip::defaultLanguageChanged, this,
                        &TracksRhiWidget::scheduleSnapshot);
                connect(singing, &SingingClip::noteChanged, this,
                        &TracksRhiWidget::scheduleSnapshot);
                connect(singing, &SingingClip::voiceContextChanged, this,
                        &TracksRhiWidget::scheduleSnapshot);
            }
        }
    }
}

void TracksRhiWidget::appendGrid(EditorRhiFrameData &frame, const double dpr) const {
    const auto visible = m_viewport.visibleSceneRect();
    const auto sceneWidth = m_viewport.tickToSceneX(appStatus->projectEditableLength);
    if (appStatus->selectedTrackIndex >= 0) {
        const auto top = appStatus->selectedTrackIndex * trackHeight * scaleY();
        EditorRhiGeometry::appendRect(
            frame.solidVertices,
            QRectF(0, top * dpr, sceneWidth * dpr, trackHeight * scaleY() * dpr),
            m_selectedTrackColor);
    }
    for (int row = 1; row <= appModel->tracks().size(); ++row) {
        const auto y = row * trackHeight * scaleY() * dpr;
        EditorRhiGeometry::appendPixelAlignedHorizontalLine(
            frame.solidVertices, y, 0.0, sceneWidth * dpr,
            withOpacity(m_commonLineColor, kRasterLineOpacity));
    }

    TimelineEmitter emitter;
    emitter.emitLines(appModel->timeline(), 128, m_viewport.startTick(), m_viewport.endTick(),
                      visible.width(), m_barLineColor, m_beatLineColor, m_commonLineColor,
                      [this, &frame, dpr, visible](const int tick, const QColor &color) {
                          EditorRhiGeometry::appendPixelAlignedVerticalLine(
                              frame.solidVertices, m_viewport.tickToSceneX(tick) * dpr,
                              visible.top() * dpr, visible.bottom() * dpr,
                              withOpacity(color, kRasterLineOpacity));
                      });
}

void TracksRhiWidget::appendClips(EditorRhiFrameData &frame, const double dpr) {
    m_clipSnapshots.clear();
    QSet<int> sampledAudioClipIds;
    const auto visiblePhysical = QRectF(m_viewport.visibleSceneRect().topLeft() * dpr,
                                        m_viewport.visibleSceneRect().size() * dpr)
                                     .adjusted(-width() * dpr, 0, width() * dpr, 0);
    const auto &tracks = appModel->tracks();
    for (int trackIndex = 0; trackIndex < tracks.size(); ++trackIndex) {
        for (const auto *clip : tracks.at(trackIndex)->clips()) {
            auto snapshot = buildClipSnapshot(clip, trackIndex, dpr);
            if (!visiblePhysical.intersects(snapshot.physicalRect))
                continue;
            if (const auto audio = qobject_cast<const AudioClip *>(clip)) {
                sampledAudioClipIds.insert(snapshot.id);
                auto &sampler = m_audioWaveformSamplers[snapshot.id];
                if (!sampler)
                    sampler = std::make_shared<AudioWaveformSampler>();
                sampler->setPath(audio->path());
                snapshot.waveform =
                    sampleAudioWaveform(*sampler, audio->audioInfo(), snapshot, dpr);
            }
            m_clipSnapshots.append(std::move(snapshot));
            appendClip(frame, m_clipSnapshots.constLast(), dpr);
        }
    }
    for (const auto &preview : m_pastePreviewSnapshots) {
        if (visiblePhysical.intersects(preview.physicalRect))
            appendClip(frame, preview, dpr);
    }
    for (auto iterator = m_audioWaveformSamplers.begin();
         iterator != m_audioWaveformSamplers.end();) {
        if (sampledAudioClipIds.contains(iterator.key()))
            ++iterator;
        else
            iterator = m_audioWaveformSamplers.erase(iterator);
    }
}

void TracksRhiWidget::appendClip(EditorRhiFrameData &frame, const ClipSnapshot &clip,
                                 const double dpr) {
    const auto &palette = *AppColorPalette::instance();
    auto fill = palette.clipBackground(clip.colorIndex);
    auto selectedFill = palette.clipBackgroundSelected(clip.colorIndex);
    auto transparent = palette.clipBackgroundTransparent(clip.colorIndex);
    auto foreground = palette.clipForeground(clip.colorIndex);
    auto border = clip.selected ? m_clipSelectedBorderColor : fill;
    if (clip.pastePreview) {
        constexpr double opacity = 0.35;
        for (auto *color : {&fill, &selectedFill, &transparent, &foreground, &border})
            color->setAlphaF(color->alphaF() * opacity);
    }
    const auto radius = 4.0 * dpr;
    const auto titleHeight = 20.0 * dpr;
    const auto preview = clipPreviewRect(clip, dpr);
    const auto hasPreview = preview.height() >= 32.0 * dpr;
    const auto bodyColor = hasPreview ? transparent : (clip.selected ? selectedFill : fill);
    EditorRhiGeometry::appendRoundedRect(frame.solidVertices, clip.physicalRect, radius, bodyColor);
    if (hasPreview) {
        const auto titleBottom = clip.physicalRect.top() + titleHeight - 1.2 * dpr - 0.75;
        const QRectF titleRect(clip.physicalRect.left(), clip.physicalRect.top(),
                               clip.physicalRect.width(),
                               std::max(0.0, titleBottom - clip.physicalRect.top()));
        const auto titleColor = clip.selected ? selectedFill : fill;
        EditorRhiGeometry::appendRoundedRect(frame.solidVertices, titleRect, radius, titleColor);
        EditorRhiGeometry::appendRect(frame.solidVertices,
                                      QRectF(titleRect.left(), titleRect.top() + radius,
                                             titleRect.width(), titleRect.height() - radius),
                                      titleColor);
    }
    if (clip.selected || clip.active) {
        EditorRhiGeometry::appendRoundedRectStroke(frame.solidVertices, clip.physicalRect, radius,
                                                   1.2 * dpr, border, 0.5);
    }

    QFont font = this->font();
    const auto logicalPixelSize =
        font.pixelSize() > 0 ? font.pixelSize() : font.pointSizeF() * logicalDpiY() / 72.0;
    font.setPixelSize(std::max(1, qRound(logicalPixelSize * dpr)));
    const QFontMetricsF metrics(font);
    const auto rawLeft = clip.physicalRect.left() - 0.6 * dpr;
    const auto rawRight = clip.physicalRect.right() + 0.6 * dpr;
    const auto visibleLeft = m_viewport.horizontalOffset() * dpr;
    const auto titleLeft =
        visibleLeft < rawLeft ? clip.physicalRect.left() : visibleLeft + 0.6 * dpr;
    const auto titleWidth = rawRight - std::max(rawLeft, visibleLeft) - 2.4 * dpr;
    constexpr double iconWidth = 4.0;
    if (metrics.horizontalAdvance(clip.title) + iconWidth * dpr <= titleWidth &&
        metrics.height() <= titleHeight) {
        const auto textTop = hasPreview ? clip.physicalRect.top()
                                        : clip.physicalRect.top() +
                                              (clip.physicalRect.height() - metrics.height()) * 0.5;
        const QRectF textClip(titleLeft + iconWidth * dpr, clip.physicalRect.top(),
                              titleWidth - iconWidth * dpr,
                              std::min(titleHeight, clip.physicalRect.height()));
        m_glyphAtlas.appendText(clip.title, font, QPointF(titleLeft + iconWidth * dpr, textTop),
                                foreground, textClip);
    }

    if (!hasPreview || preview.width() < 16.0 * dpr || preview.height() < 32.0 * dpr)
        return;
    if (clip.type == IClip::Singing && !clip.notes.isEmpty()) {
        QList<int> keys;
        keys.reserve(clip.notes.size());
        for (const auto &note : clip.notes)
            keys.append(note.key);
        const auto layout = SingingClipPreview::computeLayout(
            preview, keys, SingingClipPreview::maximumNoteHeight * dpr);
        const auto noteHeight = layout.noteHeight;
        const auto high = layout.highestKeyIndex;
        const auto contentTop = layout.contentTop;
        const auto noteColor = clip.selected ? selectedFill : fill;
        for (const auto &note : clip.notes) {
            const auto start = std::max(clip.visibleStartTick, clip.contentStartTick + note.start);
            const auto end =
                std::min(clip.visibleEndTick, clip.contentStartTick + note.start + note.length);
            if (end <= start)
                continue;
            const auto left = std::max(preview.left(), m_viewport.tickToSceneX(start) * dpr);
            const auto right = std::min(preview.right(), m_viewport.tickToSceneX(end) * dpr);
            const auto top = contentTop + (high - note.key) * noteHeight;
            EditorRhiGeometry::appendRect(frame.solidVertices,
                                          QRectF(left, top, right - left, noteHeight), noteColor);
        }

        const QRectF pianoRollRect = appStatus->pianoRollVisibleRect;
        if (clip.active && !pianoRollRect.isNull() && !pianoRollRect.isEmpty()) {
            const auto overlayStart = std::max(pianoRollRect.left() + clip.contentStartTick,
                                               static_cast<double>(clip.visibleStartTick));
            const auto overlayEnd = std::min(pianoRollRect.right() + clip.contentStartTick,
                                             static_cast<double>(clip.visibleEndTick));
            if (overlayEnd > overlayStart) {
                const auto overlayTop = contentTop + (high - pianoRollRect.bottom()) * noteHeight;
                const auto overlayBottom = contentTop + (high - pianoRollRect.top()) * noteHeight;
                const QRectF overlay(
                    m_viewport.tickToSceneX(overlayStart) * dpr, overlayTop,
                    (m_viewport.tickToSceneX(overlayEnd) - m_viewport.tickToSceneX(overlayStart)) *
                        dpr,
                    overlayBottom - overlayTop);
                const auto overlayColor = palette.clipBorder(clip.colorIndex);
                if (preview.contains(overlay)) {
                    EditorRhiGeometry::appendRoundedRectStroke(frame.solidVertices, overlay, radius,
                                                               1.2 * dpr, overlayColor);
                } else if (preview.intersects(overlay)) {
                    const auto clipped = preview.intersected(overlay);
                    const auto lineWidth = 1.2 * dpr;
                    if (overlay.left() >= preview.left() && overlay.left() <= preview.right())
                        EditorRhiGeometry::appendRect(frame.solidVertices,
                                                      QRectF(overlay.left() - lineWidth * 0.5,
                                                             clipped.top(), lineWidth,
                                                             clipped.height()),
                                                      overlayColor);
                    if (overlay.right() >= preview.left() && overlay.right() <= preview.right())
                        EditorRhiGeometry::appendRect(frame.solidVertices,
                                                      QRectF(overlay.right() - lineWidth * 0.5,
                                                             clipped.top(), lineWidth,
                                                             clipped.height()),
                                                      overlayColor);
                    if (overlay.top() >= preview.top() && overlay.top() <= preview.bottom())
                        EditorRhiGeometry::appendRect(frame.solidVertices,
                                                      QRectF(clipped.left(),
                                                             overlay.top() - lineWidth * 0.5,
                                                             clipped.width(), lineWidth),
                                                      overlayColor);
                    if (overlay.bottom() >= preview.top() && overlay.bottom() <= preview.bottom())
                        EditorRhiGeometry::appendRect(frame.solidVertices,
                                                      QRectF(clipped.left(),
                                                             overlay.bottom() - lineWidth * 0.5,
                                                             clipped.width(), lineWidth),
                                                      overlayColor);
                }
            }
        }
    } else if (clip.type == IClip::Audio) {
        const auto waveformColor = clip.selected ? selectedFill : fill;
        const auto physicalCameraOffset =
            QPointF(m_viewport.horizontalOffset(), m_viewport.verticalOffset()) * dpr;
        const auto physicalWaveformPoint = [dpr](const QPointF &point) {
            return point * dpr + QPointF(0.0, -0.5);
        };
        QVector<EditorRhiSolidVertex> waveformVertices;
        if (clip.waveform.geometry == AudioWaveformSampler::Geometry::FilledPeaks) {
            QVector<QPointF> top;
            QVector<QPointF> bottom;
            top.reserve(clip.waveform.peaks.size());
            bottom.reserve(clip.waveform.peaks.size());
            for (const auto &point : clip.waveform.peaks) {
                top.append(physicalWaveformPoint({point.x, point.yMax}));
                bottom.append(physicalWaveformPoint({point.x, point.yMin}));
            }
            EditorRhiGeometry::appendAntialiasedWaveform(waveformVertices, top, bottom, preview,
                                                         waveformColor, 0.75);

            auto flatRunStart = -1;
            for (auto index = 0; index < clip.waveform.peaks.size(); ++index) {
                const auto &point = clip.waveform.peaks[index];
                const auto flat = std::abs(point.yMax - point.yMin) < 0.5;
                if (flat && flatRunStart < 0) {
                    flatRunStart = index;
                } else if (!flat && flatRunStart >= 0) {
                    const auto runStart =
                        physicalWaveformPoint({clip.waveform.peaks[flatRunStart].x,
                                               clip.waveform.peaks[flatRunStart].yMax});
                    const auto runEnd = physicalWaveformPoint(
                        {clip.waveform.peaks[index - 1].x, clip.waveform.peaks[index - 1].yMax});
                    EditorRhiGeometry::appendPixelAlignedHorizontalLine(
                        waveformVertices, runStart.y(), runStart.x(), runEnd.x(), waveformColor);
                    flatRunStart = -1;
                }
            }
            if (flatRunStart >= 0) {
                const auto runStart = physicalWaveformPoint(
                    {clip.waveform.peaks[flatRunStart].x, clip.waveform.peaks[flatRunStart].yMax});
                const auto runEnd = physicalWaveformPoint(
                    {clip.waveform.peaks.constLast().x, clip.waveform.peaks.constLast().yMax});
                EditorRhiGeometry::appendPixelAlignedHorizontalLine(
                    waveformVertices, runStart.y(), runStart.x(), runEnd.x(), waveformColor);
            }
        } else if (clip.waveform.geometry == AudioWaveformSampler::Geometry::VerticalPeaks) {
            auto cosmeticLineOffset = 0.0;
            if (!clip.waveform.peaks.isEmpty()) {
                const auto firstPhysicalX =
                    physicalWaveformPoint({clip.waveform.peaks.constFirst().x, 0.0}).x();
                const auto firstScreenX = firstPhysicalX - physicalCameraOffset.x();
                const auto pixelPhase = firstScreenX - std::floor(firstScreenX);
                // Preserve QPainter's aliased cosmetic-pen phase after the RHI camera transform.
                cosmeticLineOffset = pixelPhase < 0.5 ? -0.5 : 0.0;
            }
            for (const auto &point : clip.waveform.peaks) {
                const auto top = physicalWaveformPoint({point.x, std::min(point.yMin, point.yMax)});
                const auto bottom =
                    physicalWaveformPoint({point.x, std::max(point.yMin, point.yMax)});
                const auto lineTop = top.y() - 0.5;
                const auto lineBottom = bottom.y() + 0.5;
                const auto lineX = std::floor(top.x() - physicalCameraOffset.x()) +
                                   physicalCameraOffset.x() + cosmeticLineOffset;
                EditorRhiGeometry::appendRect(
                    waveformVertices,
                    QRectF(lineX, lineTop, 1.0, std::max(1.0, lineBottom - lineTop)),
                    waveformColor);
            }
        } else if (clip.waveform.geometry == AudioWaveformSampler::Geometry::Curve) {
            QVector<QPointF> curve;
            curve.reserve(clip.waveform.curve.size());
            for (const auto &point : clip.waveform.curve)
                curve.append(physicalWaveformPoint(point));
            EditorRhiGeometry::appendAntialiasedHairline(waveformVertices, curve, waveformColor);
            const auto radius = clip.waveform.sampleDotRadius * dpr;
            for (const auto &point : clip.waveform.sampleDots) {
                const auto center = physicalWaveformPoint(point);
                EditorRhiGeometry::appendRoundedRect(
                    waveformVertices,
                    QRectF(center.x() - radius, center.y() - radius, radius * 2.0, radius * 2.0),
                    radius, waveformColor);
            }
        }
        EditorRhiGeometry::appendClippedTriangles(frame.solidVertices, waveformVertices, preview);
    }
}

void TracksRhiWidget::appendPlaybackIndicators(EditorRhiFrameData &frame, const double dpr) const {
    const auto visible = m_viewport.visibleSceneRect();
    const auto top = visible.top() * dpr;
    const auto bottom = visible.bottom() * dpr;
    const auto lastX = m_viewport.tickToSceneX(m_lastPlaybackPosition) * dpr;
    for (auto dashTop = top; dashTop < bottom; dashTop += 6.0 * dpr) {
        EditorRhiGeometry::appendPixelAlignedVerticalLine(
            frame.solidVertices, lastX, dashTop, std::min(dashTop + 4.0 * dpr, bottom),
            withOpacity(m_lastPlayPosIndicatorColor, kRasterLineOpacity));
    }
    EditorRhiGeometry::appendPixelAlignedVerticalLine(
        frame.solidVertices, m_viewport.tickToSceneX(m_playbackPosition) * dpr, top, bottom,
        withOpacity(m_playPosIndicatorColor, kRasterLineOpacity));
}

TracksRhiWidget::ClipSnapshot TracksRhiWidget::buildClipSnapshot(const Clip *clip,
                                                                 const int trackIndex,
                                                                 const double dpr) const {
    const auto props = previewOrModelProperties(clip);
    const auto displayTrack = m_dragPreview && m_dragPreview->clipId == clip->id()
                                  ? m_dragPreview->trackIndex
                                  : trackIndex;
    ClipSnapshot result;
    result.id = clip->id();
    result.trackIndex = displayTrack;
    result.colorIndex = appModel->tracks().at(displayTrack)->colorIndex();
    result.type = clip->clipType();
    result.contentStartTick = props.start;
    result.visibleStartTick = props.start + props.clipStart;
    result.visibleEndTick = result.visibleStartTick + props.clipLen;
    result.selected = appStatus->selectedClips.get().contains(clip->id());
    result.active = appStatus->activeClipId == clip->id();
    const auto left = m_viewport.tickToSceneX(props.start + props.clipStart) * dpr;
    const auto right = m_viewport.tickToSceneX(props.start + props.clipStart + props.clipLen) * dpr;
    result.physicalRect = QRectF(left, displayTrack * trackHeight * scaleY() * dpr, right - left,
                                 trackHeight * scaleY() * dpr)
                              .adjusted(0.6 * dpr, 1.2 * dpr, -0.6 * dpr, -1.2 * dpr);
    result.title = commonClipTitle(props, clip->id(), scaleX(), scaleY());
    if (const auto singing = qobject_cast<const SingingClip *>(clip)) {
        const auto singerName = singing->singerInfo().name();
        const auto speakerName = SpeakerMixDisplayUtils::speakerDisplayName(
            singing->singerInfo(), singing->speakerInfo(), singing->speakerMixData());
        result.title +=
            QStringLiteral("%1%2 %3 ")
                .arg(singerName.isEmpty() ? tr("(No singer)") : singerName,
                     speakerName.isEmpty() ? QString() : QStringLiteral(" / ") + speakerName,
                     singing->defaultLanguage());
        for (const auto *note : singing->notes())
            result.notes.append({note->localStart(), note->length(), note->keyIndex()});
    }
    return result;
}

QRectF TracksRhiWidget::clipPreviewRect(const ClipSnapshot &clip, const double dpr) {
    constexpr double titleHeight = 20.0;
    return {clip.physicalRect.left(), clip.physicalRect.top() + titleHeight * dpr,
            clip.physicalRect.width(), clip.physicalRect.height() - titleHeight * dpr};
}

AudioWaveformSampler::Result TracksRhiWidget::sampleAudioWaveform(AudioWaveformSampler &sampler,
                                                                  const AudioInfoModel &audioInfo,
                                                                  const ClipSnapshot &clip,
                                                                  const double dpr) const {
    const auto previewPhysical =
        QRectF(clip.physicalRect.left(), clip.physicalRect.top() + 20.0 * dpr,
               clip.physicalRect.width(), clip.physicalRect.height() - 20.0 * dpr);
    const auto previewScene = QRectF(previewPhysical.left() / dpr, previewPhysical.top() / dpr,
                                     previewPhysical.width() / dpr, previewPhysical.height() / dpr);
    const auto &timeline = appModel->timeline();
    return sampler.sample({
        .audioInfo = &audioInfo,
        .timeline = &timeline,
        .materialStartTick = clip.contentStartTick,
        .visibleStartTick = clip.visibleStartTick,
        .previewSceneRect = previewScene,
        .visibleSceneRect = m_viewport.visibleSceneRect(),
        .horizontalScale = scaleX(),
        .pixelsPerQuarterNote = pixelsPerQuarterNote,
        .leftMarginPx = m_leftMarginPx,
        .devicePixelRatio = dpr,
    });
}

void TracksRhiWidget::showTrackPastePreview(const TrackPastePreviewData &data,
                                            const int previewTick, const int baseTrackIndex) {
    m_pastePreviewSnapshots.clear();
    if (data.clips.isEmpty() || appModel->tracks().isEmpty()) {
        scheduleSnapshot();
        return;
    }

    auto firstClipStart = data.clips.first().properties.start;
    for (const auto &clip : data.clips)
        firstClipStart = std::min(firstClipStart, clip.properties.start);

    const auto dpr = devicePixelRatioF();
    m_pastePreviewSnapshots.reserve(data.clips.size());
    for (const auto &clip : data.clips) {
        const auto targetTrackIndex = std::clamp(baseTrackIndex + clip.trackIndexOffset, 0,
                                                 static_cast<int>(appModel->tracks().size()) - 1);
        const auto *targetTrack = appModel->tracks().at(targetTrackIndex);
        auto properties = clip.properties;
        properties.start = previewTick + clip.properties.start - firstClipStart;

        ClipSnapshot snapshot;
        snapshot.trackIndex = targetTrackIndex;
        snapshot.colorIndex = targetTrack->colorIndex();
        snapshot.type = clip.type;
        snapshot.contentStartTick = properties.start;
        snapshot.visibleStartTick = properties.start + properties.clipStart;
        snapshot.visibleEndTick = snapshot.visibleStartTick + properties.clipLen;
        snapshot.title = commonClipTitle(properties, -1, scaleX(), scaleY());
        snapshot.pastePreview = true;
        const auto left = m_viewport.tickToSceneX(snapshot.visibleStartTick) * dpr;
        const auto right = m_viewport.tickToSceneX(snapshot.visibleEndTick) * dpr;
        snapshot.physicalRect = QRectF(left, targetTrackIndex * trackHeight * scaleY() * dpr,
                                       right - left, trackHeight * scaleY() * dpr)
                                    .adjusted(0.6 * dpr, 1.2 * dpr, -0.6 * dpr, -1.2 * dpr);

        if (clip.type == IClip::Singing) {
            const auto singerName = targetTrack->singerInfo().name();
            const auto speakerName = SpeakerMixDisplayUtils::speakerDisplayName(
                targetTrack->singerInfo(), targetTrack->speakerInfo(),
                targetTrack->speakerMixData());
            snapshot.title +=
                QStringLiteral("%1%2 %3 ")
                    .arg(singerName.isEmpty() ? tr("(No singer)") : singerName,
                         speakerName.isEmpty() ? QString() : QStringLiteral(" / ") + speakerName,
                         clip.defaultLanguage);
            for (const auto &note : clip.notes)
                snapshot.notes.append({note.start, note.length, note.key});
        } else if (clip.type == IClip::Audio) {
            AudioWaveformSampler sampler;
            sampler.setPath(clip.audioPath);
            snapshot.waveform = sampleAudioWaveform(sampler, clip.audioInfo, snapshot, dpr);
        }
        m_pastePreviewSnapshots.append(std::move(snapshot));
    }
    scheduleSnapshot();
}

void TracksRhiWidget::clearTrackPastePreview() {
    if (m_pastePreviewSnapshots.isEmpty())
        return;
    m_pastePreviewSnapshots.clear();
    scheduleSnapshot();
}

const TracksRhiWidget::ClipSnapshot *
    TracksRhiWidget::hitTest(const QPointF &viewportPosition) const {
    const auto physicalScene = m_viewport.viewportToScene(viewportPosition) * devicePixelRatioF();
    for (auto iterator = m_clipSnapshots.crbegin(); iterator != m_clipSnapshots.crend(); ++iterator)
        if (iterator->physicalRect.contains(physicalScene))
            return &*iterator;
    return nullptr;
}

double TracksRhiWidget::wheelDelta(const QWheelEvent *event, const bool preferHorizontal) const {
    const auto angle = event->angleDelta();
    if (preferHorizontal && angle.x() != 0)
        return angle.x();
    if (angle.y() != 0)
        return angle.y();
    const auto pixel = event->pixelDelta();
    return (preferHorizontal && pixel.x() != 0 ? pixel.x() : pixel.y()) * 4.0;
}

int TracksRhiWidget::trackIndexAt(const QPointF &viewportPosition) const {
    const auto unit = m_viewport.sceneYToUnit(m_viewport.viewportToScene(viewportPosition).y());
    const auto index = static_cast<int>(std::floor(unit));
    return index >= 0 && index < appModel->tracks().size() ? index : -1;
}

int TracksRhiWidget::tickAt(const QPointF &viewportPosition) const {
    return qRound(m_viewport.sceneXToTick(m_viewport.viewportToScene(viewportPosition).x()));
}

int TracksRhiWidget::snapTick(const int tick) const {
    return TimelineSnapUtils::snapNearest(tick, snapStep(tick), appModel->timeline());
}

int TracksRhiWidget::snapStep(const int tick) const {
    TimelineEmitter emitter;
    const auto pixelsPerTick =
        scaleX() * TracksEditorGlobal::pixelsPerQuarterNote / AppGlobal::ticksPerQuarterNote;
    const auto ticksPerPixel = pixelsPerTick > 0.0 ? 1.0 / pixelsPerTick : 0.0;
    return emitter.gridStep(appModel->timeline(), 128, ticksPerPixel, tick);
}

void TracksRhiWidget::beginClipDrag(const ClipSnapshot &clip, const QMouseEvent *event) {
    Track *track = nullptr;
    const auto modelClip = appModel->findClipById(clip.id, track);
    if (!modelClip || !track)
        return;
    const auto physicalScene = m_viewport.viewportToScene(event->position()) * devicePixelRatioF();
    const auto relativeX = physicalScene.x() - clip.physicalRect.left();
    const auto tolerance = AppGlobal::resizeTolerance * devicePixelRatioF();
    if (relativeX <= tolerance)
        m_dragMode = DragMode::ResizeLeft;
    else if (relativeX >= clip.physicalRect.width() - tolerance)
        m_dragMode = DragMode::ResizeRight;
    else
        m_dragMode = DragMode::Move;
    m_mouseDownScene = m_viewport.viewportToScene(event->position());
    m_mouseDownProperties = Clip::ClipCommonProperties(*modelClip);
    m_mouseDownTrackIndex = appModel->tracks().indexOf(track);
    m_dragPreview = DragPreview{clip.id, m_mouseDownTrackIndex, m_mouseDownProperties};
    m_dragMoved = false;
    appStatus->currentEditObject = AppStatus::EditObjectType::Clip;
}

void TracksRhiWidget::updateDrag(const QPointF &position, const Qt::KeyboardModifiers modifiers) {
    if (!m_dragPreview)
        return;
    auto properties = m_mouseDownProperties;
    const auto scene = m_viewport.viewportToScene(position);
    const auto deltaTicks =
        qRound(m_viewport.sceneXToTick(scene.x()) - m_viewport.sceneXToTick(m_mouseDownScene.x()));
    const auto quantizeDisabled = modifiers == Qt::AltModifier;
    const auto snap = [this, quantizeDisabled](const int value) {
        return quantizeDisabled ? value : snapTick(value);
    };
    const auto originalLeft = m_mouseDownProperties.start + m_mouseDownProperties.clipStart;
    const auto originalRight = originalLeft + m_mouseDownProperties.clipLen;
    if (m_dragMode == DragMode::Move) {
        const auto left = snap(originalLeft + deltaTicks);
        properties.start = left - properties.clipStart;
        const auto track = trackIndexAt(position);
        if (track >= 0)
            m_dragPreview->trackIndex = track;
    } else if (m_dragMode == DragMode::ResizeLeft) {
        const auto left = snap(originalLeft + deltaTicks);
        if (left < originalRight) {
            properties.clipStart = std::max(0, left - properties.start);
            properties.clipLen = originalRight - (properties.start + properties.clipStart);
        }
    } else if (m_dragMode == DragMode::ResizeRight) {
        const auto right = snap(originalRight + deltaTicks);
        if (const auto *clip = appModel->findClipById(m_dragPreview->clipId)) {
            const auto *singing = qobject_cast<const SingingClip *>(clip);
            auto contentLength = properties.length;
            if (singing) {
                contentLength = AppGlobal::ticksPerWholeNote;
                if (singing->notes().count() > 0) {
                    const auto *lastNote = *singing->notes().rbegin();
                    contentLength = lastNote->localStart() + lastNote->length();
                }
            }
            ClipResizeUtils::updateRightEdge(properties, right - originalLeft, singing != nullptr,
                                             contentLength);
        }
    }
    m_dragPreview->properties = properties;
    m_dragMoved = true;
    scheduleSnapshot();
}

void TracksRhiWidget::commitDrag() {
    if (m_dragPreview && m_dragMoved) {
        auto properties = m_dragPreview->properties;
        if (const auto audio =
                qobject_cast<AudioClip *>(appModel->findClipById(m_dragPreview->clipId)))
            AudioClip::preserveUnchangedTruth(properties, m_mouseDownProperties);
        trackController->onClipPropertyChanged(properties, m_dragPreview->trackIndex);
    }
    m_dragPreview.reset();
    m_dragMode = DragMode::None;
    m_dragMoved = false;
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
}

void TracksRhiWidget::discardDrag() {
    m_dragPreview.reset();
    m_dragMode = DragMode::None;
    m_dragMoved = false;
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
    scheduleSnapshot();
}

void TracksRhiWidget::syncSelection(const QList<int> &ids, const int preferredTrack) const {
    appStatus->selectedClips = ids;
    if (preferredTrack >= 0)
        appStatus->selectedTrackIndex = preferredTrack;
}

void TracksRhiWidget::updateCursor(const QPointF &position) {
    const auto *clip = hitTest(position);
    if (!clip) {
        unsetCursor();
        return;
    }
    const auto physical = m_viewport.viewportToScene(position).x() * devicePixelRatioF();
    const auto relative = physical - clip->physicalRect.left();
    const auto tolerance = AppGlobal::resizeTolerance * devicePixelRatioF();
    setCursor(relative <= tolerance || relative >= clip->physicalRect.width() - tolerance
                  ? Qt::SizeHorCursor
                  : Qt::ArrowCursor);
}

void TracksRhiWidget::setAutoPageTurn(const bool enabled) {
    m_autoTurnPage = enabled;
    if (enabled)
        handleAutoPageTurn();
}

void TracksRhiWidget::handleAutoPageTurn() {
    if (!m_autoTurnPage || !m_autoPageTurnAvailable ||
        appStatus->currentEditObject != AppStatus::EditObjectType::None) {
        return;
    }

    const auto start = startTick();
    const auto end = endTick();
    const auto range = end - start;
    if (range <= 0.0)
        return;
    if (m_playbackPosition > end) {
        m_viewport.setStartTick(m_playbackPosition > end + range ? m_playbackPosition : end);
    } else if (m_playbackPosition < start) {
        m_viewport.setStartTick(m_playbackPosition);
    }
}

void TracksRhiWidget::updateAutoPageTurnAvailability() {
    const bool available =
        AutoPageTurnUtils::isPageDurationAvailable(appModel->timeline(), startTick(), endTick());
    if (m_autoPageTurnAvailable != available) {
        m_autoPageTurnAvailable = available;
        emit autoPageTurnAvailabilityChanged(available);
    }
}

Clip::ClipCommonProperties TracksRhiWidget::previewOrModelProperties(const Clip *clip) const {
    if (m_dragPreview && m_dragPreview->clipId == clip->id())
        return m_dragPreview->properties;
    return Clip::ClipCommonProperties(*clip);
}

QColor TracksRhiWidget::barLineColor() const {
    return m_barLineColor;
}

QColor TracksRhiWidget::backgroundColor() const {
    return m_backgroundColor;
}

void TracksRhiWidget::setBackgroundColor(const QColor &color) {
    m_backgroundColor = color;
    scheduleSnapshot();
}

void TracksRhiWidget::setBarLineColor(const QColor &color) {
    m_barLineColor = color;
    scheduleSnapshot();
}

QColor TracksRhiWidget::beatLineColor() const {
    return m_beatLineColor;
}

void TracksRhiWidget::setBeatLineColor(const QColor &color) {
    m_beatLineColor = color;
    scheduleSnapshot();
}

QColor TracksRhiWidget::commonLineColor() const {
    return m_commonLineColor;
}

void TracksRhiWidget::setCommonLineColor(const QColor &color) {
    m_commonLineColor = color;
    scheduleSnapshot();
}

QColor TracksRhiWidget::playPosIndicatorColor() const {
    return m_playPosIndicatorColor;
}

void TracksRhiWidget::setPlayPosIndicatorColor(const QColor &color) {
    m_playPosIndicatorColor = color;
    scheduleSnapshot();
}

QColor TracksRhiWidget::lastPlayPosIndicatorColor() const {
    return m_lastPlayPosIndicatorColor;
}

void TracksRhiWidget::setLastPlayPosIndicatorColor(const QColor &color) {
    m_lastPlayPosIndicatorColor = color;
    scheduleSnapshot();
}

QColor TracksRhiWidget::selectedTrackColor() const {
    return m_selectedTrackColor;
}

void TracksRhiWidget::setSelectedTrackColor(const QColor &color) {
    m_selectedTrackColor = color;
    scheduleSnapshot();
}

QColor TracksRhiWidget::clipSelectedBorderColor() const {
    return m_clipSelectedBorderColor;
}

void TracksRhiWidget::setClipSelectedBorderColor(const QColor &color) {
    m_clipSelectedBorderColor = color;
    scheduleSnapshot();
}

QColor TracksRhiWidget::rubberBandBorderColor() const {
    return m_rubberBandBorderColor;
}

void TracksRhiWidget::setRubberBandBorderColor(const QColor &color) {
    m_rubberBandBorderColor = color;
    scheduleSnapshot();
}

QColor TracksRhiWidget::rubberBandFillColor() const {
    return m_rubberBandFillColor;
}

void TracksRhiWidget::setRubberBandFillColor(const QColor &color) {
    m_rubberBandFillColor = color;
    scheduleSnapshot();
}
