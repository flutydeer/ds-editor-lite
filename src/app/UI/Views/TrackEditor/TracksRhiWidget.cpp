#include "TracksRhiWidget.h"

#include "Controller/EditorViewController.h"
#include "Controller/PlaybackController.h"
#include "Controller/TrackController.h"
#include "Global/AppGlobal.h"
#include "Global/TracksEditorGlobal.h"
#include "Model/AppStatus/AppStatus.h"
#include "UI/Utils/AppColorPalette.h"
#include "UI/Utils/ITimelinePainter.h"
#include "UI/Utils/SpeakerMixDisplayUtils.h"

#include <lite/MusicBase/TimelineSnapUtils.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <functional>

using namespace TracksEditorGlobal;

namespace {
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
        void drawBar(QPainter *, const int tick, int) override {
            m_callback(tick, m_bar);
        }

        void drawBeat(QPainter *, const int tick, int, int) override {
            m_callback(tick, m_beat);
        }

        void drawSubdivision(QPainter *painter, const int tick, int, int) override {
            auto color = m_common;
            color.setAlphaF(color.alphaF() * painter->opacity());
            m_callback(tick, color);
        }

        QColor m_bar;
        QColor m_beat;
        QColor m_common;
        Callback m_callback;
    };

    QRectF insetRect(const QRectF &rect, const double inset) {
        return rect.adjusted(inset, inset, -inset, -inset);
    }
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
    m_viewport.setVerticalContent(appModel->tracks().size(), trackHeight);

    connect(&m_viewport, &EditorViewportController::scaleChanged, this,
            &TracksRhiWidget::scaleChanged);
    connect(&m_viewport, &EditorViewportController::timeRangeChanged, this,
            &TracksRhiWidget::timeRangeChanged);
    connect(&m_viewport, &EditorViewportController::scrollChanged, this,
            [this](const double, const double vertical) {
                emit verticalOffsetChanged(vertical);
                emit visibleRectChanged(logicalVisibleRect());
            });
    connect(&m_viewport, &EditorViewportController::viewportChanged, this,
            &TracksRhiWidget::scheduleSnapshot);

    connect(appModel, &AppModel::modelChanged, this, [this] {
        rebuildModelConnections();
        m_viewport.setVerticalContent(appModel->tracks().size(), trackHeight);
        scheduleSnapshot();
    });
    connect(appModel, &AppModel::trackChanged, this, [this] {
        rebuildModelConnections();
        m_viewport.setVerticalContent(appModel->tracks().size(), trackHeight);
        scheduleSnapshot();
    });
    connect(appModel, &AppModel::trackMoved, this, [this] {
        rebuildModelConnections();
        scheduleSnapshot();
    });
    connect(appModel, &AppModel::timelineChanged, this, &TracksRhiWidget::scheduleSnapshot);
    connect(appStatus, &AppStatus::selectedTrackIndexChanged, this,
            &TracksRhiWidget::scheduleSnapshot);
    connect(appStatus, &AppStatus::clipSelectionChanged, this, &TracksRhiWidget::scheduleSnapshot);
    connect(appStatus, &AppStatus::activeClipIdChanged, this, &TracksRhiWidget::scheduleSnapshot);
    connect(appStatus, &AppStatus::projectEditableLengthChanged, this,
            &TracksRhiWidget::setSceneLength);
    rebuildModelConnections();
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
}

void TracksRhiWidget::setPlaybackPosition(const double tick) {
    m_playbackPosition = tick;
    scheduleSnapshot();
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
        editorViewController->centerPianoRollAt(playbackController->position(), 60);
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

void TracksRhiWidget::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        discardDrag();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        trackController->onRemoveClips(appStatus->selectedClips.get());
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
    m_glyphAtlas.clear();
    scheduleSnapshot();
}

void TracksRhiWidget::rebuildSnapshot() {
    const auto dpr = devicePixelRatioF();
    EditorRhiFrameData frame;
    frame.clearColor = QColor(30, 32, 36);
    frame.physicalCameraOffset =
        QPointF(m_viewport.horizontalOffset(), m_viewport.verticalOffset()) * dpr;
    m_glyphAtlas.beginFrame();
    appendGrid(frame, dpr);
    appendClips(frame, dpr);
    appendPlaybackIndicators(frame, dpr);
    if (m_dragMode == DragMode::RectSelect) {
        const QRectF rect(m_rubberBandStart * dpr, m_rubberBandEnd * dpr);
        auto fill = QColor(155, 186, 255, 64);
        EditorRhiGeometry::appendRect(frame.solidVertices, rect.normalized(), fill);
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
            connect(clip, &Clip::propertyChanged, this, &TracksRhiWidget::scheduleSnapshot);
            if (const auto *singing = qobject_cast<const SingingClip *>(clip)) {
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
    const auto sceneHeight = appModel->tracks().size() * trackHeight * scaleY();
    if (appStatus->selectedTrackIndex >= 0) {
        const auto top = appStatus->selectedTrackIndex * trackHeight * scaleY();
        EditorRhiGeometry::appendRect(
            frame.solidVertices,
            QRectF(0, top * dpr, sceneWidth * dpr, trackHeight * scaleY() * dpr),
            m_selectedTrackColor);
    }
    for (int row = 0; row <= appModel->tracks().size(); ++row) {
        const auto y = row * trackHeight * scaleY() * dpr;
        EditorRhiGeometry::appendPixelAlignedHorizontalLine(frame.solidVertices, y, 0.0,
                                                            sceneWidth * dpr, m_commonLineColor);
    }

    TimelineEmitter emitter;
    emitter.emitLines(appModel->timeline(), 128, m_viewport.startTick(), m_viewport.endTick(),
                      visible.width(), m_barLineColor, m_beatLineColor, m_commonLineColor,
                      [this, &frame, dpr, sceneHeight](const int tick, const QColor &color) {
                          EditorRhiGeometry::appendPixelAlignedVerticalLine(
                              frame.solidVertices, m_viewport.tickToSceneX(tick) * dpr, 0.0,
                              sceneHeight * dpr, color);
                      });
}

void TracksRhiWidget::appendClips(EditorRhiFrameData &frame, const double dpr) {
    m_clipSnapshots.clear();
    const auto visiblePhysical = QRectF(m_viewport.visibleSceneRect().topLeft() * dpr,
                                        m_viewport.visibleSceneRect().size() * dpr)
                                     .adjusted(-width() * dpr, 0, width() * dpr, 0);
    const auto &tracks = appModel->tracks();
    for (int trackIndex = 0; trackIndex < tracks.size(); ++trackIndex) {
        for (const auto *clip : tracks.at(trackIndex)->clips()) {
            auto snapshot = buildClipSnapshot(clip, trackIndex, dpr);
            if (!visiblePhysical.intersects(snapshot.physicalRect))
                continue;
            m_clipSnapshots.append(std::move(snapshot));
            appendClip(frame, m_clipSnapshots.constLast(), dpr);
        }
    }
    for (const auto &preview : m_pastePreviewSnapshots) {
        if (visiblePhysical.intersects(preview.physicalRect))
            appendClip(frame, preview, dpr);
    }
}

void TracksRhiWidget::appendClip(EditorRhiFrameData &frame, const ClipSnapshot &clip,
                                 const double dpr) {
    const auto &palette = *AppColorPalette::instance();
    auto fill = clip.selected ? palette.clipBackgroundSelected(clip.colorIndex)
                              : palette.clipBackground(clip.colorIndex);
    auto transparent = palette.clipBackgroundTransparent(clip.colorIndex);
    auto foreground = palette.clipForeground(clip.colorIndex);
    auto border = clip.selected ? m_clipSelectedBorderColor : palette.clipBorder(clip.colorIndex);
    if (clip.pastePreview) {
        constexpr double opacity = 0.35;
        for (auto *color : {&fill, &transparent, &foreground, &border})
            color->setAlphaF(color->alphaF() * opacity);
    }
    const auto radius = 4.0 * dpr;
    EditorRhiGeometry::appendRoundedRect(frame.solidVertices, clip.physicalRect, radius,
                                         clip.physicalRect.height() >= 36.0 * dpr ? transparent
                                                                                  : fill);
    if (clip.selected || clip.active) {
        EditorRhiGeometry::appendRoundedRect(frame.solidVertices, clip.physicalRect, radius,
                                             border);
        EditorRhiGeometry::appendRoundedRect(frame.solidVertices,
                                             insetRect(clip.physicalRect, 1.25 * dpr),
                                             std::max(0.0, radius - 1.25 * dpr), fill);
    }
    const auto titleHeight = std::min(24.0 * dpr, clip.physicalRect.height());
    if (clip.physicalRect.height() >= 36.0 * dpr) {
        EditorRhiGeometry::appendRoundedRect(
            frame.solidVertices,
            QRectF(clip.physicalRect.left(), clip.physicalRect.top(), clip.physicalRect.width(),
                   titleHeight + radius),
            radius, fill);
        EditorRhiGeometry::appendRect(frame.solidVertices,
                                      QRectF(clip.physicalRect.left(),
                                             clip.physicalRect.top() + titleHeight,
                                             clip.physicalRect.width(), radius),
                                      fill);
    }

    QFont font = this->font();
    font.setPixelSize(std::max(8, qRound(12.0 * dpr)));
    const auto textLeft = std::max(clip.physicalRect.left() + 5.0 * dpr,
                                   m_viewport.horizontalOffset() * dpr + 4.0 * dpr);
    m_glyphAtlas.appendText(clip.title, font,
                            QPointF(textLeft, clip.physicalRect.top() + 3.0 * dpr), foreground,
                            QRectF(clip.physicalRect.left() + 2.0 * dpr, clip.physicalRect.top(),
                                   clip.physicalRect.width() - 4.0 * dpr, titleHeight));

    const QRectF preview(clip.physicalRect.left() + dpr,
                         clip.physicalRect.top() + titleHeight + dpr,
                         clip.physicalRect.width() - 2.0 * dpr,
                         clip.physicalRect.height() - titleHeight - 2.0 * dpr);
    if (preview.width() < 8.0 || preview.height() < 8.0)
        return;
    if (clip.type == IClip::Singing && !clip.notes.isEmpty()) {
        auto low = 127;
        auto high = 0;
        for (const auto &note : clip.notes) {
            low = std::min(low, note.key);
            high = std::max(high, note.key);
        }
        const auto noteHeight = std::min(16.0 * dpr, preview.height() / (high - low + 1));
        for (const auto &note : clip.notes) {
            const auto start = std::max(clip.visibleStartTick, clip.contentStartTick + note.start);
            const auto end =
                std::min(clip.visibleEndTick, clip.contentStartTick + note.start + note.length);
            if (end <= start)
                continue;
            const auto left = m_viewport.tickToSceneX(start) * dpr;
            const auto right = m_viewport.tickToSceneX(end) * dpr;
            const auto top = preview.top() + (high - note.key) * noteHeight;
            EditorRhiGeometry::appendRect(frame.solidVertices,
                                          QRectF(left, top, right - left, noteHeight), fill);
        }
    } else if (clip.type == IClip::Audio && !clip.peaks.isEmpty()) {
        const auto count = clip.peaks.size();
        const auto center = preview.center().y();
        const auto halfHeight = preview.height() * 0.48;
        const auto firstPixel = std::max(0, qRound(clip.physicalRect.left()));
        const auto lastPixel = std::max(firstPixel + 1, qRound(clip.physicalRect.right()));
        for (int x = firstPixel; x < lastPixel; ++x) {
            const auto ratio =
                (x - clip.physicalRect.left()) / std::max(1.0, clip.physicalRect.width());
            const auto index =
                std::clamp<qsizetype>(qRound(ratio * static_cast<double>(count - 1)), 0, count - 1);
            const auto [minimum, maximum] = clip.peaks.at(index);
            const auto top = center - maximum / 32768.0 * halfHeight;
            const auto bottom = center - minimum / 32768.0 * halfHeight;
            EditorRhiGeometry::appendPixelAlignedVerticalLine(frame.solidVertices, x, top, bottom,
                                                              fill);
        }
    }
}

void TracksRhiWidget::appendPlaybackIndicators(EditorRhiFrameData &frame, const double dpr) const {
    const auto height = appModel->tracks().size() * trackHeight * scaleY() * dpr;
    EditorRhiGeometry::appendPixelAlignedVerticalLine(
        frame.solidVertices, m_viewport.tickToSceneX(m_lastPlaybackPosition) * dpr, 0.0, height,
        m_lastPlayPosIndicatorColor);
    EditorRhiGeometry::appendPixelAlignedVerticalLine(
        frame.solidVertices, m_viewport.tickToSceneX(m_playbackPosition) * dpr, 0.0, height,
        m_playPosIndicatorColor);
}

TracksRhiWidget::ClipSnapshot TracksRhiWidget::buildClipSnapshot(const Clip *clip,
                                                                 const int trackIndex,
                                                                 const double dpr) const {
    const auto track = appModel->tracks().at(trackIndex);
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
    result.title = props.name;
    if (const auto singing = qobject_cast<const SingingClip *>(clip)) {
        const auto singerName = singing->singerInfo().name();
        const auto speakerName = SpeakerMixDisplayUtils::speakerDisplayName(
            singing->singerInfo(), singing->speakerInfo(), singing->speakerMixData());
        result.title += QStringLiteral("  %1%2").arg(
            singerName.isEmpty() ? tr("(No singer)") : singerName,
            speakerName.isEmpty() ? QString() : QStringLiteral(" / ") + speakerName);
        for (const auto *note : singing->notes())
            result.notes.append({note->localStart(), note->length(), note->keyIndex()});
    } else if (const auto audio = qobject_cast<const AudioClip *>(clip)) {
        result.peaks = audio->audioInfo().peakCacheMipmap.isEmpty()
                           ? audio->audioInfo().peakCache.toVector()
                           : audio->audioInfo().peakCacheMipmap.toVector();
    }
    Q_UNUSED(track);
    return result;
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
        snapshot.title = properties.name;
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
            snapshot.title += QStringLiteral("  %1%2").arg(
                singerName.isEmpty() ? tr("(No singer)") : singerName,
                speakerName.isEmpty() ? QString() : QStringLiteral(" / ") + speakerName);
            for (const auto &note : clip.notes)
                snapshot.notes.append({note.start, note.length, note.key});
        } else if (clip.type == IClip::Audio) {
            snapshot.peaks = clip.audioInfo.peakCacheMipmap.isEmpty()
                                 ? clip.audioInfo.peakCache.toVector()
                                 : clip.audioInfo.peakCacheMipmap.toVector();
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
        if (right > originalLeft)
            properties.clipLen = right - originalLeft;
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

Clip::ClipCommonProperties TracksRhiWidget::previewOrModelProperties(const Clip *clip) const {
    if (m_dragPreview && m_dragPreview->clipId == clip->id())
        return m_dragPreview->properties;
    return Clip::ClipCommonProperties(*clip);
}

QColor TracksRhiWidget::barLineColor() const {
    return m_barLineColor;
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
