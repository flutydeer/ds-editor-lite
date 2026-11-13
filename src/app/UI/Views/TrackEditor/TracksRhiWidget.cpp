#include "TracksRhiWidget.h"

#include <lite/ProjectModel/Utils/ClipResizeUtils.h>
#include "SingingClipPreviewLayout.h"
#include "Controller/AudioDecodingController.h"
#include "Controller/EditorViewController.h"
#include "UI/Views/Common/EditorSelectionUtils.h"
#include "Utils/UiLanguageManager.h"
#include "Controller/PlaybackController.h"
#include "Controller/TrackController.h"
#include "Global/AppGlobal.h"
#include "Global/TracksEditorGlobal.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/EditSessionManager.h"
#include "UI/Utils/AppColorPalette.h"
#include "UI/Utils/WaveformRhiUtils.h"
#include "UI/Views/Common/TimelineLineEmitter.h"
#include "UI/Utils/SpeakerMixDisplayUtils.h"
#include "UI/Views/Common/AutoPageTurnUtils.h"
#include "UI/Views/Common/EditorItemGeometry.h"
#include "UI/Views/Common/EditorResizeUtils.h"
#include "UI/Views/Common/EditorPointerUtils.h"
#include "UI/Views/Common/EditorRhiScrollBarController.h"
#include "UI/Views/Common/EditorPenController.h"
#include "UI/Views/Common/EditorTouchController.h"
#include "UI/Views/Common/EditorWheelController.h"
#include "UI/Views/Common/AudioLoadHint.h"
#include "UI/Views/Common/ClipTitleInlineEdit.h"

#include <lite/GUI/Theme/ThemeManager.h>
#include <lite/MusicBase/TimelineSnapUtils.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QApplication>
#include <QCoreApplication>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFontMetricsF>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLocale>
#include <QMetaObject>
#include <QMimeData>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QSet>
#include <QShowEvent>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <numbers>

using namespace TracksEditorGlobal;

namespace {
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

}

TracksRhiWidget::TracksRhiWidget(QWidget *parent)
    : EditorRhiWidget(QStringLiteral("TracksRhi"), parent), m_viewport(nullptr),
      m_glyphAtlas(QSize(1024, 1024), 4) {
    setObjectName(QStringLiteral("TracksRhiWidget"));
    setMouseTracking(true);
    setAcceptDrops(true);
    setAttribute(Qt::WA_AcceptTouchEvents);
    setAttribute(Qt::WA_TouchPadAcceptSingleTouchEvents);
    m_viewport.setPixelsPerQuarterNote(pixelsPerQuarterNote);
    m_viewport.setScaleBounds(0.0001, 10000.0, 0.575, 8.0);
    m_viewport.setEnsureContentFillsViewport(true, false);
    m_baseSceneLength = std::max(0, appStatus->projectEditableLength.get());
    m_viewport.setContentTickRange(0.0, effectiveSceneLength());
    // One extra unit for the virtual append slot at the bottom of the canvas
    m_viewport.setVerticalContent(appModel->tracks().size() + 1, trackHeight);
    m_wheelController = std::make_unique<EditorWheelController>(&m_viewport, this, this);
    m_touchController = new EditorTouchController(this, this);
    m_penController = new EditorPenController(this, this);

    m_scrollBars = new EditorRhiScrollBarController(this, this);
    connect(m_scrollBars, &EditorRhiScrollBarController::offsetChangeRequested, this,
            [this](const QPointF &offset) {
                m_wheelController->stop();
                m_viewport.scrollBy({offset.x() - m_viewport.horizontalOffset(),
                                     offset.y() - m_viewport.verticalOffset()});
            });

    // Inline clip title rename. The session commits itself; the canvas only
    // keeps the snapshot's painted title in step with it.
    m_titleEditor = new ClipTitleInlineEdit(this, this);
    connect(m_titleEditor, &ClipTitleInlineEdit::sessionFinished, this, [this](const int clipId) {
        if (m_titleEditingClipId != clipId)
            return;
        m_titleEditingClipId = -1;
        scheduleSnapshot();
    });
    // Every external event that moves the target ends the session.
    connect(&m_viewport, &EditorViewportController::viewportChanged, this,
            &TracksRhiWidget::finishClipTitleEditing);
    connect(this, &TracksRhiWidget::sizeChanged, this,
            &TracksRhiWidget::finishClipTitleEditing);

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
    connect(&m_edgeAutoScroller, &EdgeAutoScroller::frame, this, [this](const double dtMs) {
        if (m_externalDragActive)
            onExternalDropScrollFrame(dtMs);
        else
            onDragAutoScrollFrame(dtMs);
    });

    // A theme switch re-reads every color during the next frame rebuild; without this
    // the widget keeps blitting the last frame texture with the old palette until the
    // next interaction.
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this,
            &TracksRhiWidget::scheduleSnapshot);
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
    // Anything outside the canvas can replace or clear the selection (delete, cut, undo, select all),
    // so the range anchor follows the platform selection instead of remembering a clip that is gone.
    connect(appStatus, &AppStatus::clipSelectionChanged, this,
            [this](const QList<int> &selected) { m_selectionModel.synchronize(selected); });
    connect(appStatus, &AppStatus::activeClipIdChanged, this, &TracksRhiWidget::scheduleSnapshot);
    connect(appStatus, &AppStatus::pianoRollVisibleRectChanged, this,
            &TracksRhiWidget::scheduleSnapshot);
    connect(appStatus, &AppStatus::pianoRollNoteEditPreviewChanged, this,
            &TracksRhiWidget::scheduleSnapshot);
    connect(appStatus, &AppStatus::pianoRollNoteErasePreviewChanged, this,
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
    rebuildModelConnections();
    m_hintTicker.setInterval(16);
    connect(&m_hintTicker, &QTimer::timeout, this, &TracksRhiWidget::scheduleSnapshot);
    QTimer::singleShot(0, this, [this] {
        updateScrollBars();
        updateAutoPageTurnAvailability();
    });
}

TracksRhiWidget::~TracksRhiWidget() {
    finishDragSession(EditSessionEndReason::Discard);
}

TrackPanelViewState TracksRhiWidget::viewState() const {
    const auto state = m_viewport.state();
    return {state.centerTick, state.centerUnit - 0.5, state.horizontalScale, state.verticalScale};
}

bool TracksRhiWidget::centerAt(const double tick, const double trackIndex) {
    if (!std::isfinite(tick) || !std::isfinite(trackIndex))
        return false;
    m_wheelController->stop();
    return m_viewport.centerAt(tick, trackIndex + 0.5);
}

bool TracksRhiWidget::setViewScale(const double horizontalScale, const double verticalScale) {
    if (!std::isfinite(horizontalScale) || !std::isfinite(verticalScale) ||
        horizontalScale <= 0.0 || verticalScale <= 0.0) {
        return false;
    }
    m_wheelController->stop();
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
    return m_viewport.logicalVisibleSceneRect().contains(focusRect)
               ? HistoryFocusVisibility::Visible
               : HistoryFocusVisibility::ScrollRequired;
}

bool TracksRhiWidget::revealFocus(const HistoryFocus &focus, const bool animated) {
    if (!focus.isValid() || focus.kind != HistoryFocusKind::TrackClips)
        return false;
    m_wheelController->stop();
    const auto focusBounds = [this, &focus] {
        QRectF bounds;
        for (const auto id : focus.objectIds) {
            int trackIndex = -1;
            if (const auto *clip = appModel->findClipById(id, trackIndex)) {
                const auto left = m_viewport.tickToSceneX(clip->start() + clip->clipStart());
                const auto right =
                    m_viewport.tickToSceneX(clip->start() + clip->clipStart() + clip->clipLen());
                const QRectF rect(left, m_viewport.unitToSceneY(trackIndex),
                                  std::max(1.0, right - left), trackHeight * scaleY());
                bounds = bounds.isNull() ? rect : bounds.united(rect);
            }
        }
        if (!bounds.isNull())
            return bounds;
        int trackIndex = focus.trackIndex;
        if (focus.trackId >= 0)
            appModel->findTrackById(focus.trackId, trackIndex);
        if (trackIndex < 0)
            trackIndex = qRound((focus.valueStart + focus.valueEnd) * 0.5);
        if (trackIndex < 0)
            return QRectF();
        const auto left = m_viewport.tickToSceneX(focus.tickStart);
        const auto right = m_viewport.tickToSceneX(focus.tickEnd);
        return QRectF(left, m_viewport.unitToSceneY(trackIndex), std::max(1.0, right - left),
                      trackHeight * scaleY());
    };

    auto bounds = focusBounds();
    if (!bounds.isValid() || bounds.isNull())
        return false;
    constexpr double margin = 24.0;
    const auto viewportSize = m_viewport.viewportSize();
    const auto availableWidth = std::max(1.0, viewportSize.width() - margin * 2.0);
    const auto availableHeight = std::max(1.0, viewportSize.height() - margin * 2.0);
    auto targetScaleX = scaleX();
    auto targetScaleY = scaleY();
    if (bounds.width() > availableWidth)
        targetScaleX *= availableWidth / bounds.width();
    if (bounds.height() > availableHeight)
        targetScaleY *= availableHeight / bounds.height();
    targetScaleX = m_viewport.boundedScale(Qt::Horizontal, targetScaleX);
    targetScaleY = m_viewport.boundedScale(Qt::Vertical, targetScaleY);
    if (!m_viewport.setScale(targetScaleX, targetScaleY,
                             QPointF(viewportSize.width() * 0.5, viewportSize.height() * 0.5))) {
        return false;
    }
    bounds = focusBounds();
    if (!m_viewport.ensureVisible(bounds, margin, margin, animated))
        return false;
    return m_viewport.logicalVisibleSceneRect().contains(bounds);
}

QRectF TracksRhiWidget::logicalVisibleRect() const {
    return m_viewport.logicalVisibleSceneRect();
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
    m_baseSceneLength = std::max(0, tick);
    m_viewport.setContentTickRange(0.0, effectiveSceneLength());
    updateAutoPageTurnAvailability();
}

int TracksRhiWidget::effectiveSceneLength() const {
    return m_baseSceneLength + m_sceneLengthExtension;
}

void TracksRhiWidget::setSceneLengthExtension(const int ticks) {
    const auto extension = std::max(0, ticks);
    if (m_sceneLengthExtension == extension)
        return;
    m_sceneLengthExtension = extension;
    m_viewport.setContentTickRange(0.0, effectiveSceneLength());
}

void TracksRhiWidget::setLeftMarginPx(const double px) {
    m_leftMarginPx = px;
    m_viewport.setLeftMarginPx(px);
}

void TracksRhiWidget::setPlaybackPosition(const double tick) {
    m_playbackPosition = tick;
    handleAutoPageTurn();
    if (!m_snapshotScheduled)
        updatePlaybackOverlay();
}

void TracksRhiWidget::setLastPlaybackPosition(const double tick) {
    m_lastPlaybackPosition = tick;
    scheduleSnapshot();
}

void TracksRhiWidget::onWheelHorScale(QWheelEvent *event) {
    m_wheelController->horizontalScale(event);
}

void TracksRhiWidget::onWheelVerScale(QWheelEvent *event) {
    m_wheelController->verticalScale(event);
}

void TracksRhiWidget::onWheelHorScroll(QWheelEvent *event) {
    m_wheelController->horizontalScroll(event);
}

void TracksRhiWidget::onWheelVerScroll(QWheelEvent *event) {
    m_wheelController->verticalScroll(event);
}

void TracksRhiWidget::setVerticalOffset(const double value) {
    m_wheelController->stop();
    m_viewport.scrollBy({0.0, value - m_viewport.verticalOffset()});
}

void TracksRhiWidget::updateScrollBars() {
    if (!m_scrollBars)
        return;
    m_scrollBars->setMetrics(QSizeF(m_viewport.tickToSceneX(effectiveSceneLength()),
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

bool TracksRhiWidget::event(QEvent *event) {
    // The pen layer first: it only claims tablet events, and it has to see them
    // before anything else decides what they mean.
    if (m_penController->handleEvent(event))
        return true;
    if (m_touchController->handleEvent(event))
        return true;
    if (m_wheelController->handleTouchpadEvent(event))
        return true;
    if (event->type() == QEvent::WindowDeactivate) {
        m_touchController->cancel();
        m_penController->interrupt();
        discardDrag();
    }
    if (event->type() == QEvent::LanguageChange)
        scheduleSnapshot(); // rebuild clip snapshots with the new display language
    if (event->type() == QEvent::NativeGesture &&
        m_wheelController->handleNativeGesture(static_cast<QNativeGestureEvent *>(event))) {
        return true;
    }
    return EditorRhiWidget::event(event);
}

void TracksRhiWidget::showEvent(QShowEvent *event) {
    EditorRhiWidget::showEvent(event);
    updateAutoPageTurnAvailability();
}

void TracksRhiWidget::hideEvent(QHideEvent *event) {
    m_touchController->cancel();
    m_penController->interrupt();
    if (m_dragMode != DragMode::None)
        discardDrag();
    else
        disarmDragAutoScroll();
    if (m_externalDragActive)
        endExternalDropOverlay();
    EditorRhiWidget::hideEvent(event);
    updateAutoPageTurnAvailability();
}

void TracksRhiWidget::wheelEvent(QWheelEvent *event) {
    if (!m_wheelController->handleWheel(event))
        EditorRhiWidget::wheelEvent(event);
}

void TracksRhiWidget::mousePressEvent(QMouseEvent *event) {
    m_lastPointerPosition = event->position();
    m_wheelController->stop();
    setFocus(Qt::MouseFocusReason);
    m_viewport.stopAnimation();
    disarmDragAutoScroll();
    const auto *hit = hitTest(event->position());
    if (event->button() != Qt::LeftButton) {
        if (hit) {
            // A right press only decides what the menu acts on: plain semantics, so a group that
            // already contains the clip survives and the menu can edit all of it.
            updateClipSelection(*hit, Qt::NoModifier);
            scheduleSnapshot();
        } else {
            m_selectionModel.cancelPress();
            syncSelection({});
        }
        EditorRhiWidget::mousePressEvent(event);
        return;
    }
    if (hit) {
        if (updateClipSelection(*hit, event->modifiers()))
            beginClipDrag(*hit, event);
    } else {
        m_selectionModel.cancelPress();
        syncSelection({});
        m_dragMode = DragMode::RectSelect;
        m_rubberBandStart = m_viewport.viewportToScene(event->position());
        m_rubberBandEnd = m_rubberBandStart;
    }
    prepareDragAutoScroll(event->position());
    scheduleSnapshot();
    event->accept();
}

void TracksRhiWidget::mouseMoveEvent(QMouseEvent *event) {
    m_lastPointerPosition = event->position();
    if (m_dragMode == DragMode::RectSelect) {
        updateRubberBandSelection(event->position());
    } else if (m_dragMode != DragMode::None) {
        updateDrag(event->position(), event->modifiers());
    } else {
        updateCursor(event->position());
    }
    updateDragAutoScrollState(event->position());
    event->accept();
}

void TracksRhiWidget::mouseReleaseEvent(QMouseEvent *event) {
    m_lastPointerPosition = event->position();
    if (event->button() == Qt::LeftButton) {
        if (m_dragMode == DragMode::RectSelect) {
            m_dragMode = DragMode::None;
            m_selectionModel.cancelPress();
        } else if (m_dragMode != DragMode::None) {
            commitDrag();
        }
        disarmDragAutoScroll();
        setSceneLengthExtension(0);
        scheduleSnapshot();
    }
    event->accept();
}

void TracksRhiWidget::stopTouchViewportAnimation() {
    m_wheelController->stop();
    m_viewport.stopAnimation();
}

void TracksRhiWidget::panTouchViewportBy(const QPointF &deltaPixels) {
    // The content follows the finger, so the scroll offset moves the other way.
    m_viewport.scrollBy(-deltaPixels);
}

void TracksRhiWidget::zoomTouchViewportBy(const double horizontalFactor,
                                          const double verticalFactor, const QPointF &anchor) {
    const auto horizontal =
        m_viewport.boundedScale(Qt::Horizontal, m_viewport.horizontalScale() * horizontalFactor);
    const auto vertical =
        m_viewport.boundedScale(Qt::Vertical, m_viewport.verticalScale() * verticalFactor);
    m_viewport.setScale(horizontal, vertical, anchor);
}

EditorTouchTarget::ContentHit
    TracksRhiWidget::touchContentAt(const QPointF &viewportPosition) const {
    const auto *hit = hitTest(viewportPosition);
    if (!hit)
        return ContentHit::None;
    // A finger has to select a clip before it can move or resize it, so that
    // dragging across the arrangement scrolls instead of shuffling clips.
    return hit->selected ? ContentHit::Selected : ContentHit::Unselected;
}

EditorTouchTarget::BlankDragAction TracksRhiWidget::touchBlankDragAction() const {
    // Empty canvas in the arrangement view carries nothing to create, so a
    // plain finger drag there scrolls. Rubber band selection stays reachable
    // through a long press.
    return BlankDragAction::Pan;
}

EditorPenEraser TracksRhiWidget::penEraserAction() const {
    return EditorPenPolicy::arrangement();
}

void TracksRhiWidget::cancelTouchPointerInteraction() {
    if (m_dragMode == DragMode::RectSelect)
        m_dragMode = DragMode::None;
    else if (m_dragMode != DragMode::None)
        discardDrag();
    disarmDragAutoScroll();
    setSceneLengthExtension(0);
    scheduleSnapshot();
}

bool TracksRhiWidget::touchRelayTextBegin(const QPointF &viewportPosition) {
    return m_titleEditor && m_titleEditor->relayTouchBegin(viewportPosition);
}

void TracksRhiWidget::touchRelayTextMove(const QPointF &viewportPosition) {
    if (m_titleEditor)
        m_titleEditor->relayTouchMove(viewportPosition);
}

void TracksRhiWidget::touchRelayTextEnd(const QPointF &viewportPosition) {
    if (m_titleEditor)
        m_titleEditor->relayTouchEnd(viewportPosition);
}

void TracksRhiWidget::touchRelayTextCancel() {
    if (m_titleEditor)
        m_titleEditor->relayTouchCancel();
}

void TracksRhiWidget::updateRubberBandSelection(const QPointF &position) {
    m_rubberBandEnd = m_viewport.viewportToScene(position);
    const auto dpr = devicePixelRatioF();
    const QRectF selection = QRectF(m_rubberBandStart * dpr, m_rubberBandEnd * dpr).normalized();
    QList<int> ids;
    const auto &tracks = appModel->tracks();
    for (int trackIndex = 0; trackIndex < tracks.size(); ++trackIndex) {
        for (const auto *clip : tracks.at(trackIndex)->clips()) {
            if (selection.intersects(
                    clipModelPhysicalRect(previewOrModelProperties(clip), trackIndex, dpr))) {
                ids.append(clip->id());
            }
        }
    }
    syncSelection(ids);
    scheduleSnapshot();
}

Qt::Orientations TracksRhiWidget::dragAutoScrollAxes() const {
    if (m_dragMode == DragMode::Move || m_dragMode == DragMode::RectSelect)
        return Qt::Horizontal | Qt::Vertical;
    if (m_dragMode == DragMode::ResizeLeft || m_dragMode == DragMode::ResizeRight)
        return Qt::Horizontal;
    return {};
}

void TracksRhiWidget::prepareDragAutoScroll(const QPointF &pressPosition) {
    m_edgeAutoScroller.prepareDrag(pressPosition);
}

void TracksRhiWidget::disarmDragAutoScroll() {
    m_edgeAutoScroller.stopDrag();
}

void TracksRhiWidget::updateDragAutoScrollState(const QPointF &pointerPosition) {
    const auto axes = dragAutoScrollAxes();
    if (!axes) {
        disarmDragAutoScroll();
        return;
    }

    const QRectF viewportRect(QPointF(), size());
    m_edgeAutoScroller.setDragAxes(axes);
    m_edgeAutoScroller.updateDragState(pointerPosition, viewportRect,
                                       QApplication::startDragDistance());
}

void TracksRhiWidget::onDragAutoScrollFrame(const double dtMs) {
    if (!m_edgeAutoScroller.isDragArmed() || !EditorPointer::isPointerPressed() || !isVisible()) {
        disarmDragAutoScroll();
        return;
    }

    const auto pointerPosition = m_lastPointerPosition;
    const QRectF viewportRect(QPointF(), size());
    const auto step = m_edgeAutoScroller.computeDragStep(pointerPosition, viewportRect, dtMs);
    if (step.x() > 0 && (m_dragMode == DragMode::Move || m_dragMode == DragMode::ResizeRight)) {
        const auto maximumOffset =
            std::max(0.0, m_viewport.tickToSceneX(effectiveSceneLength()) - width());
        if (m_viewport.horizontalOffset() >= maximumOffset - 1.0) {
            const auto visibleTicks = std::max(1, qRound(endTick() - startTick()));
            setSceneLengthExtension(m_sceneLengthExtension + visibleTicks);
        }
    }
    if (!step.isNull())
        m_viewport.scrollBy(step);

    const auto clamped = EdgeAutoScroller::clampToRect(pointerPosition, viewportRect);
    if (m_dragMode == DragMode::RectSelect)
        updateRubberBandSelection(clamped);
    else if (m_dragMode != DragMode::None)
        updateDrag(clamped, QGuiApplication::queryKeyboardModifiers());
    updateDragAutoScrollState(pointerPosition);
}

void TracksRhiWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    if (const auto *hit = hitTest(event->position())) {
        // The title band renames the clip in place, the body keeps opening it
        // in the clip editor.
        if (event->button() == Qt::LeftButton && !hit->pastePreview &&
            titleBandViewportRect(*hit).contains(event->position())) {
            beginClipTitleEdit(*hit);
            event->accept();
            return;
        }
        trackController->setActiveClip(hit->id);
        editorViewController->showBottomPanelPage(QStringLiteral("ClipEditor"));
        double targetTick = playbackController->position();
        double targetKey = 60.0;
        if (hit->type == IClip::Singing && !hit->notes.isEmpty()) {
            const auto dpr = devicePixelRatioF();
            const auto physicalPosition = m_viewport.viewportToScene(event->position()) * dpr;
            const auto preview = clipPreviewRect(*hit, dpr);
            const auto layout = SingingClipPreview::computeLayout(
                preview, SingingClipPreview::keyIndices(hit->notes),
                SingingClipPreview::maximumNoteHeight * dpr);
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
        updateClipSelection(*hit, Qt::NoModifier);
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
        syncSelection({});
        scheduleSnapshot();
        context.target = TrackEditorMenuContext::Target::Background;
    }
    emit contextMenuRequested(context);
    event->accept();
}

void TracksRhiWidget::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        m_wheelController->stop();
        event->acceptProposedAction();
        m_edgeAutoScroller.stopDrag();
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
    m_edgeAutoScroller.stopDrag();
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
    const auto sceneWidth = m_viewport.tickToSceneX(effectiveSceneLength());
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

void TracksRhiWidget::appendSplitPreviewOverlay(EditorRhiFrameData &frame, const double dpr) const {
    if (!m_splitPreviewActive || m_splitPreviewTrackIndex < 0)
        return;

    // Same forked-line geometry as the piano roll's split tool indicator
    constexpr double extensionLength = 8.0;
    constexpr double forkLength = 6.0;
    constexpr double forkAngle = 45.0;
    const auto trackHeightPx = trackHeight * scaleY();
    const auto x = m_viewport.tickToSceneX(m_splitPreviewTick) * dpr;
    const auto lineTop = (m_splitPreviewTrackIndex * trackHeightPx - extensionLength) * dpr;
    const auto lineBottom = ((m_splitPreviewTrackIndex + 1) * trackHeightPx + extensionLength) * dpr;
    const auto forkAngleRad = forkAngle * std::numbers::pi / 180.0;
    const auto forkOffsetX = forkLength * std::sin(forkAngleRad) * dpr;
    const auto forkOffsetY = forkLength * std::cos(forkAngleRad) * dpr;
    const QColor color(255, 100, 100);

    const auto appendSegment = [&frame, dpr, &color](const QPointF &from, const QPointF &to) {
        EditorRhiGeometry::appendAntialiasedStroke(frame.solidVertices, {from, to}, 2.0 * dpr,
                                                   color);
    };
    appendSegment({x, lineTop}, {x, lineBottom});
    appendSegment({x, lineTop}, {x - forkOffsetX, lineTop - forkOffsetY});
    appendSegment({x, lineTop}, {x + forkOffsetX, lineTop - forkOffsetY});
    appendSegment({x, lineBottom}, {x - forkOffsetX, lineBottom + forkOffsetY});
    appendSegment({x, lineBottom}, {x + forkOffsetX, lineBottom + forkOffsetY});
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
    const auto hasSelection = !appStatus->selectedClips.get().isEmpty();
    const auto step = event->modifiers() == Qt::AltModifier ? 1 : nudgeStep();
    if (hasSelection) {
        switch (event->key()) {
            case Qt::Key_Left:
            case Qt::Key_Right:
                nudgeSelection(event->key() == Qt::Key_Left ? -step : step, 0);
                event->accept();
                return;
            case Qt::Key_Up:
            case Qt::Key_Down:
                nudgeSelection(0, event->key() == Qt::Key_Up ? -1 : 1);
                event->accept();
                return;
            default:
                break;
        }
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
    auto frame = acquireFrame();
    frame.solidVertices.clear();
    frame.drawList.clear();
    frame.clearColor = m_backgroundColor;
    frame.physicalCameraOffset =
        QPointF(m_viewport.horizontalOffset(), m_viewport.verticalOffset()) * dpr;
    m_glyphAtlas.beginFrame();
    appendGrid(frame, dpr);
    appendClips(frame, dpr);
    // The loading ring turns on its own, so the ticker runs only while a visible clip still has no
    // waveform to draw.
    const bool hintAnimated = std::any_of(
        m_clipSnapshots.cbegin(), m_clipSnapshots.cend(), [](const ClipSnapshot &snapshot) {
            return snapshot.loadState == AppGlobal::AudioClipLoadState::Loading;
        });
    if (hintAnimated && !m_hintTicker.isActive())
        m_hintTicker.start();
    else if (!hintAnimated && m_hintTicker.isActive())
        m_hintTicker.stop();
    appendLastPlaybackIndicator(frame, dpr);
    appendDropOverlay(frame, dpr);
    appendSplitPreviewOverlay(frame, dpr);
    if (m_dragMode == DragMode::RectSelect) {
        const auto rect = QRectF(m_rubberBandStart * dpr, m_rubberBandEnd * dpr).normalized();
        const auto radius = EditorItemGeometry::adaptiveCornerRadius(rect, 6.0 * dpr);
        EditorRhiGeometry::appendRoundedRect(frame.solidVertices, rect, radius,
                                             m_rubberBandFillColor);
        EditorRhiGeometry::appendRoundedRectStroke(frame.solidVertices, rect, radius, 1.5 * dpr,
                                                   m_rubberBandBorderColor);
    }
    frame.drawList.finish(frame.solidVertices.size());
    m_glyphAtlas.populateTextureBatches(frame.textureBatches);
    submitFrame(std::move(frame));
    updatePlaybackOverlay();
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
                // A property write moves or repaints the target, which ends an
                // open rename. The commit path has already closed the session,
                // so this only fires for changes coming from elsewhere.
                if (clip->id() == m_titleEditingClipId)
                    finishClipTitleEditing();
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
            if (const auto *audio = qobject_cast<const AudioClip *>(clip)) {
                connect(audio, &AudioClip::pathStatusChanged, this,
                        &TracksRhiWidget::scheduleSnapshot);
            }
        }
    }
    // Load-state edges carry no model signal: a resolution starts without touching the clip and a
    // decode ends by writing audioInfo, so the canvas listens to the controller directly.
    if (!m_loadStateConnected) {
        m_loadStateConnected = true;
        connect(audioDecodingController, &AudioDecodingController::loadStateChanged, this,
                &TracksRhiWidget::scheduleSnapshot);
    }
}

void TracksRhiWidget::appendGrid(EditorRhiFrameData &frame, const double dpr) const {
    const auto visible = m_viewport.visibleSceneRect();
    const auto sceneWidth = m_viewport.tickToSceneX(effectiveSceneLength());
    if (appStatus->selectedTrackIndex >= 0) {
        const auto top = appStatus->selectedTrackIndex * trackHeight * scaleY();
        EditorRhiGeometry::appendRect(
            frame.solidVertices,
            QRectF(0, top * dpr, sceneWidth * dpr, trackHeight * scaleY() * dpr),
            m_selectedTrackColor);
    }
    for (int row = 1; row <= appModel->tracks().size(); ++row) {
        const auto y = row * trackHeight * scaleY() * dpr;
        EditorRhiGeometry::appendAntialiasedHorizontalLine(frame.solidVertices, y, 0.0,
                                                           sceneWidth * dpr, dpr, m_commonLineColor,
                                                           m_viewport.verticalOffset() * dpr);
    }

    TimelineLineEmitter emitter;
    emitter.emitLines(appModel->timeline(), 128, m_viewport.startTick(), m_viewport.endTick(),
                      visible.width(), m_barLineColor, m_beatLineColor, m_commonLineColor,
                      [this, &frame, dpr, visible](const int tick, const QColor &color) {
                          EditorRhiGeometry::appendAntialiasedVerticalLine(
                              frame.solidVertices, m_viewport.tickToSceneX(tick) * dpr,
                              visible.top() * dpr, visible.bottom() * dpr, dpr, color,
                              m_viewport.horizontalOffset() * dpr);
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
            if (!visiblePhysical.intersects(snapshot.paintPhysicalRect))
                continue;
            if (const auto audio = qobject_cast<const AudioClip *>(clip)) {
                if (snapshot.loadState == AppGlobal::AudioClipLoadState::Loaded) {
                    sampledAudioClipIds.insert(snapshot.id);
                    auto &sampler = m_audioWaveformSamplers[snapshot.id];
                    if (!sampler)
                        sampler = std::make_shared<AudioWaveformSampler>();
                    sampler->setPath(audio->path());
                    snapshot.waveform =
                        sampleAudioWaveform(*sampler, audio->audioInfo(), snapshot, dpr);
                }
            }
            m_clipSnapshots.append(std::move(snapshot));
            appendClip(frame, m_clipSnapshots.constLast(), dpr);
        }
    }
    for (const auto &preview : m_pastePreviewSnapshots) {
        if (visiblePhysical.intersects(preview.paintPhysicalRect))
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
    const auto &paintRect = clip.paintPhysicalRect;
    const auto radius = EditorItemGeometry::adaptiveCornerRadius(
        paintRect, EditorItemGeometry::clipCornerRadius * dpr);
    const auto titleHeight = EditorItemGeometry::clipTitleHeight * dpr;
    const auto preview = clipPreviewRect(clip, dpr);
    // The title owns its own strip only while the body below it is tall enough
    // for a preview. The test runs on logical units so both backends share it.
    const auto hasPreview =
        EditorItemGeometry::clipHasTitleStrip(EditorItemGeometry::scaledRect(paintRect, 1.0 / dpr));
    const auto bodyColor = hasPreview ? transparent : (clip.selected ? selectedFill : fill);
    EditorRhiGeometry::appendRoundedRect(frame.solidVertices, paintRect, radius, bodyColor);
    if (hasPreview) {
        const auto titleBottom =
            paintRect.top() + titleHeight - EditorItemGeometry::clipVerticalPadding * dpr;
        const QRectF titleRect(paintRect.left(), paintRect.top(), paintRect.width(),
                               std::max(0.0, titleBottom - paintRect.top()));
        const auto titleColor = clip.selected ? selectedFill : fill;
        EditorRhiGeometry::appendTopRoundedRect(frame.solidVertices, titleRect, radius, titleColor);
    }
    if (clip.selected || clip.active) {
        EditorRhiGeometry::appendRoundedRectStroke(frame.solidVertices, paintRect, radius,
                                                   EditorItemGeometry::clipBorderWidth * dpr,
                                                   border, 0.5);
    }

    QFont font = this->font();
    const QFontMetricsF metrics(font);
    const auto rawLeft = clip.modelPhysicalRect.left();
    const auto rawRight = clip.modelPhysicalRect.right();
    const auto visibleLeft = m_viewport.horizontalOffset() * dpr;
    const auto titleLeft = visibleLeft < rawLeft
                               ? paintRect.left()
                               : visibleLeft + EditorItemGeometry::clipHorizontalPadding * dpr;
    const auto titleWidth =
        rawRight - std::max(rawLeft, visibleLeft) - EditorItemGeometry::clipBorderWidth * 2.0 * dpr;
    constexpr double iconWidth = 4.0;
    // An open inline rename is the only place the name shows.
    if (!clip.titleEditing &&
        metrics.horizontalAdvance(clip.title) * dpr + iconWidth * dpr <= titleWidth &&
        metrics.height() * dpr <= titleHeight) {
        const auto textTop =
            hasPreview ? paintRect.top()
                       : paintRect.top() + (paintRect.height() - metrics.height() * dpr) * 0.5;
        const QRectF textClip(titleLeft + iconWidth * dpr, paintRect.top(),
                              titleWidth - iconWidth * dpr,
                              hasPreview ? titleHeight : paintRect.height());
        const auto textSpan = m_glyphAtlas.appendText(
            clip.title, font, QPointF(titleLeft + iconWidth * dpr, textTop), foreground, textClip,
            dpr, QPointF(m_viewport.horizontalOffset(), m_viewport.verticalOffset()) * dpr,
            physicalWindowOffset());
        frame.drawList.appendTexture(textSpan, frame.solidVertices.size());
    }

    if (!hasPreview || preview.width() < 16.0 * dpr || preview.height() < 32.0 * dpr)
        return;
    if (clip.type == IClip::Singing && !clip.notes.isEmpty()) {
        const auto layout =
            SingingClipPreview::computeLayout(preview, SingingClipPreview::keyIndices(clip.notes),
                                              SingingClipPreview::maximumNoteHeight * dpr);
        const auto noteHeight = layout.noteHeight;
        const auto high = layout.highestKeyIndex;
        const auto contentTop = layout.contentTop;
        const auto noteColor = clip.selected ? selectedFill : fill;
        for (const auto &note : clip.notes) {
            const auto start = std::max(clip.visibleStartTick, clip.contentStartTick + note.rStart);
            const auto end =
                std::min(clip.visibleEndTick, clip.contentStartTick + note.rStart + note.length);
            if (end <= start)
                continue;
            const auto left = std::max(preview.left(), m_viewport.tickToSceneX(start) * dpr);
            const auto right = std::min(preview.right(), m_viewport.tickToSceneX(end) * dpr);
            const auto top = contentTop + (high - note.keyIndex) * noteHeight;
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
                QVector<EditorRhiSolidVertex> overlayVertices;
                EditorRhiGeometry::appendRoundedRectStroke(overlayVertices, overlay, radius,
                                                           1.2 * dpr, overlayColor);
                EditorRhiGeometry::appendClippedTriangles(frame.solidVertices, overlayVertices,
                                                          preview);
            }
        }
    } else if (clip.type == IClip::Audio) {
        const auto waveformColor = clip.selected ? selectedFill : fill;
        if (clip.loadState != AppGlobal::AudioClipLoadState::Loaded) {
            const auto isMissing = clip.loadState == AppGlobal::AudioClipLoadState::Missing;
            auto textColor = waveformColor;
            if (isMissing)
                textColor.setAlphaF(textColor.alphaF() * 0.5);
            const auto text = isMissing
                                  ? QCoreApplication::translate("AudioClipView", "File missing")
                                  : QCoreApplication::translate("AudioClipView", "Loading...");
            const QFont font = this->font();
            const QFontMetricsF metrics(font);
            const QSizeF textSize(std::ceil(metrics.horizontalAdvance(text) * dpr),
                                  std::ceil(metrics.height() * dpr));
            // Centre inside the visible slice of the clip: on a long clip scrolled away from its
            // middle, the geometric centre is off-screen and the hint would never be seen.
            auto labelRect = preview;
            const auto visiblePhysical = QRectF(m_viewport.visibleSceneRect().topLeft() * dpr,
                                                m_viewport.visibleSceneRect().size() * dpr);
            const auto clippedLabel = labelRect.intersected(visiblePhysical);
            if (clippedLabel.isEmpty())
                return;
            labelRect = clippedLabel;
            auto topLeft =
                labelRect.center() - QPointF(textSize.width() * 0.5, textSize.height() * 0.5);
            if (!isMissing) {
                // Same layout and same centreline as the QGraphics backend, only in physical pixels
                const auto block = AudioLoadHint::layout(labelRect, textSize.width(), dpr);
                auto trackColor = textColor;
                trackColor.setAlphaF(trackColor.alphaF() * AudioLoadHint::trackAlpha);
                QVector<EditorRhiSolidVertex> ringVertices;
                EditorRhiGeometry::appendAntialiasedStroke(
                    ringVertices,
                    AudioLoadHint::trackCenterline(block.ringCenter, block.ringRadius),
                    AudioLoadHint::ringStrokeWidth * dpr, trackColor);
                EditorRhiGeometry::appendAntialiasedStroke(
                    ringVertices,
                    AudioLoadHint::arcCenterline(block.ringCenter, block.ringRadius,
                                                 AudioLoadHint::clockMs()),
                    AudioLoadHint::ringStrokeWidth * dpr, textColor);
                EditorRhiGeometry::appendClippedTriangles(frame.solidVertices, ringVertices,
                                                          labelRect);
                topLeft.setX(block.textLeft);
            }
            const auto textSpan = m_glyphAtlas.appendText(
                text, font, topLeft, textColor, labelRect, dpr,
                QPointF(m_viewport.horizontalOffset(), m_viewport.verticalOffset()) * dpr,
                physicalWindowOffset());
            frame.drawList.appendTexture(textSpan, frame.solidVertices.size());
            return;
        }
        WaveformRhiUtils::appendWaveform(
            frame.solidVertices, clip.waveform, preview, waveformColor, dpr,
            QPointF(m_viewport.horizontalOffset(), m_viewport.verticalOffset()) * dpr);
    }
}

void TracksRhiWidget::appendLastPlaybackIndicator(EditorRhiFrameData &frame,
                                                  const double dpr) const {
    const auto visible = m_viewport.visibleSceneRect();
    const auto top = visible.top() * dpr;
    const auto bottom = visible.bottom() * dpr;
    const auto lastX = m_viewport.tickToSceneX(m_lastPlaybackPosition) * dpr;
    for (auto dashTop = top; dashTop < bottom; dashTop += 6.0 * dpr) {
        EditorRhiGeometry::appendAntialiasedVerticalLine(
            frame.solidVertices, lastX, dashTop, std::min(dashTop + 4.0 * dpr, bottom), dpr,
            m_lastPlayPosIndicatorColor, m_viewport.horizontalOffset() * dpr);
    }
}

void TracksRhiWidget::updatePlaybackOverlay() {
    const auto dpr = devicePixelRatioF();
    m_playbackOverlayRects.clear();
    EditorRhiGeometry::appendAntialiasedVerticalOverlay(
        m_playbackOverlayRects,
        (m_viewport.tickToSceneX(m_playbackPosition) - m_viewport.horizontalOffset()) * dpr, 0.0,
        height() * dpr, dpr, m_playPosIndicatorColor);
    m_playbackOverlayRects = submitOverlay(std::move(m_playbackOverlayRects));
}

TracksRhiWidget::ClipSnapshot TracksRhiWidget::buildClipSnapshot(const Clip *clip,
                                                                 const int trackIndex,
                                                                 const double dpr) const {
    const auto props = previewOrModelProperties(clip);
    const auto placement = m_placements.constFind(clip->id());
    const auto displayTrack = placement != m_placements.cend() ? placement->trackIndex : trackIndex;
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
    result.modelPhysicalRect = clipModelPhysicalRect(props, displayTrack, dpr);
    result.paintPhysicalRect = EditorItemGeometry::clipPaintRect(result.modelPhysicalRect, dpr);
    result.title = commonClipTitle(props, clip->id(), scaleX(), scaleY());
    result.titleEditing = clip->id() == m_titleEditingClipId;
    if (const auto audio = qobject_cast<const AudioClip *>(clip)) {
        result.loadState = audioDecodingController->loadState(audio);
    }
    if (const auto singing = qobject_cast<const SingingClip *>(clip)) {
        const auto singerName =
            singing->singerInfo().displayName(UiLanguageManager::currentBcp47Candidates());
        const auto speakerName = SpeakerMixDisplayUtils::speakerDisplayName(
            singing->singerInfo(), singing->speakerInfo(), singing->speakerMixData());
        result.title +=
            QStringLiteral("%1%2 %3 ")
                .arg(singerName.isEmpty() ? tr("(No singer)") : singerName,
                     speakerName.isEmpty() ? QString() : QStringLiteral(" / ") + speakerName,
                     singing->defaultLanguage());
        for (const auto *note : singing->notes()) {
            result.notes.append({note->id(), note->localStart(), note->length(), note->keyIndex()});
        }
        result.notes = SingingClipPreview::projectNotes(result.notes, result.active,
                                                        appStatus->pianoRollNoteEditPreview.get(),
                                                        appStatus->pianoRollNoteErasePreview.get());
    }
    return result;
}

QRectF TracksRhiWidget::clipModelPhysicalRect(const Clip::ClipCommonProperties &properties,
                                              const int trackIndex, const double dpr) const {
    const auto left = m_viewport.tickToSceneX(properties.start + properties.clipStart) * dpr;
    const auto right =
        m_viewport.tickToSceneX(properties.start + properties.clipStart + properties.clipLen) * dpr;
    return {left, trackIndex * trackHeight * scaleY() * dpr, right - left,
            trackHeight * scaleY() * dpr};
}

QRectF TracksRhiWidget::clipPreviewRect(const ClipSnapshot &clip, const double dpr) {
    const auto titleHeight = EditorItemGeometry::clipTitleHeight * dpr;
    return {clip.paintPhysicalRect.left(), clip.paintPhysicalRect.top() + titleHeight,
            clip.paintPhysicalRect.width(), clip.paintPhysicalRect.height() - titleHeight};
}

AudioWaveformSampler::Result TracksRhiWidget::sampleAudioWaveform(AudioWaveformSampler &sampler,
                                                                  const AudioInfoModel &audioInfo,
                                                                  const ClipSnapshot &clip,
                                                                  const double dpr) const {
    const auto previewPhysical = clipPreviewRect(clip, dpr);
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
        snapshot.modelPhysicalRect = clipModelPhysicalRect(properties, targetTrackIndex, dpr);
        snapshot.paintPhysicalRect =
            EditorItemGeometry::clipPaintRect(snapshot.modelPhysicalRect, dpr);

        if (clip.type == IClip::Singing) {
            const auto singerName =
                targetTrack->singerInfo().displayName(UiLanguageManager::currentBcp47Candidates());
            const auto speakerName = SpeakerMixDisplayUtils::speakerDisplayName(
                targetTrack->singerInfo(), targetTrack->speakerInfo(),
                targetTrack->speakerMixData());
            snapshot.title +=
                QStringLiteral("%1%2 %3 ")
                    .arg(singerName.isEmpty() ? tr("(No singer)") : singerName,
                         speakerName.isEmpty() ? QString() : QStringLiteral(" / ") + speakerName,
                         clip.defaultLanguage);
            for (const auto &note : clip.notes)
                snapshot.notes.append({-1, note.start, note.length, note.key});
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

void TracksRhiWidget::showTrackSplitPreview(const TrackSplitPreviewData &data) {
    if (data.trackIndex < 0)
        return;
    if (m_splitPreviewActive && m_splitPreviewTrackIndex == data.trackIndex &&
        m_splitPreviewTick == data.splitTick)
        return;
    m_splitPreviewActive = true;
    m_splitPreviewTrackIndex = data.trackIndex;
    m_splitPreviewTick = data.splitTick;
    scheduleSnapshot();
}

void TracksRhiWidget::clearTrackSplitPreview() {
    if (!m_splitPreviewActive)
        return;
    m_splitPreviewActive = false;
    m_splitPreviewTrackIndex = -1;
    m_splitPreviewTick = 0;
    scheduleSnapshot();
}

const TracksRhiWidget::ClipSnapshot *
    TracksRhiWidget::hitTest(const QPointF &viewportPosition) const {
    const auto physicalScene = m_viewport.viewportToScene(viewportPosition) * devicePixelRatioF();
    for (auto iterator = m_clipSnapshots.crbegin(); iterator != m_clipSnapshots.crend(); ++iterator)
        if (iterator->modelPhysicalRect.contains(physicalScene))
            return &*iterator;
    return nullptr;
}

int TracksRhiWidget::trackIndexAt(const QPointF &viewportPosition) const {
    const auto unit = m_viewport.sceneYToUnit(m_viewport.viewportToScene(viewportPosition).y());
    const auto index = static_cast<int>(std::floor(unit));
    return index >= 0 && index < appModel->tracks().size() ? index : -1;
}

QRectF TracksRhiWidget::titleBandViewportRect(const ClipSnapshot &clip) const {
    const auto dpr = devicePixelRatioF();
    if (dpr <= 0.0)
        return {};
    // Paint geometry is physical; the title band is expressed in logical units
    // so both backends read the same constants.
    const auto logicalPaintRect = EditorItemGeometry::scaledRect(clip.paintPhysicalRect, 1.0 / dpr);
    if (!EditorItemGeometry::clipHasTitleStrip(logicalPaintRect))
        return {};
    const auto band = EditorItemGeometry::clipTitleBand(logicalPaintRect);
    if (band.isEmpty())
        return {};
    return band.translated(-m_viewport.offset());
}

void TracksRhiWidget::beginClipTitleEdit(const ClipSnapshot &clip) {
    if (!m_titleEditor || clip.id < 0 || clip.pastePreview)
        return;
    const auto band = titleBandViewportRect(clip);
    if (band.isEmpty())
        return;
    const auto *modelClip = appModel->findClipById(clip.id);
    if (!modelClip)
        return;
    // The snapshot reads the id, so marking it before the session opens keeps
    // the painted title off the frame that lands under the editor.
    m_titleEditingClipId = clip.id;
    scheduleSnapshot();
    m_titleEditor->begin(clip.id, band, modelClip->name(), font());
}

void TracksRhiWidget::finishClipTitleEditing() {
    if (m_titleEditor)
        m_titleEditor->submit();
}

void TracksRhiWidget::cancelClipTitleEditing() {
    if (m_titleEditor)
        m_titleEditor->cancel();
}

int TracksRhiWidget::tickAt(const QPointF &viewportPosition) const {
    return qRound(m_viewport.sceneXToTick(m_viewport.viewportToScene(viewportPosition).x()));
}

int TracksRhiWidget::snapTick(const int tick) const {
    return TimelineSnapUtils::snapNearest(tick, snapStep(tick), appModel->timeline());
}

int TracksRhiWidget::snapStep(const int tick) const {
    TimelineLineEmitter emitter;
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
    const auto relativeX = physicalScene.x() - clip.modelPhysicalRect.left();
    const auto tolerance = EditorPointer::resizeTolerance() * devicePixelRatioF();
    const auto edge =
        EditorResizeUtils::horizontalEdgeAt(relativeX, clip.modelPhysicalRect.width(), tolerance);
    auto gesture = ClipSelectionDrag::Gesture::Move;
    if (edge == EditorResizeUtils::HorizontalEdge::Left) {
        gesture = ClipSelectionDrag::Gesture::ResizeLeft;
        m_dragMode = DragMode::ResizeLeft;
    } else if (edge == EditorResizeUtils::HorizontalEdge::Right) {
        gesture = ClipSelectionDrag::Gesture::ResizeRight;
        m_dragMode = DragMode::ResizeRight;
    } else {
        m_dragMode = DragMode::Move;
    }

    m_dragAnchorId = clip.id;
    m_dragTrackIndex = appModel->tracks().indexOf(track);
    QList<ClipSelectionDrag::ClipState> states;
    captureSelectionStates(states);
    ClipSelectionDrag::Session::Environment environment;
    environment.trackCount = static_cast<int>(appModel->tracks().size());
    environment.timeline = &appModel->timeline();
    environment.gridStepAt = [this](const int tick) { return snapStep(tick); };
    const auto pressScene = m_viewport.viewportToScene(event->position());
    m_dragSession.begin(gesture, m_dragAnchorId, states, environment,
                        m_viewport.sceneXToTick(pressScene.x()), m_dragTrackIndex);
    m_dragMoved = false;
    clearDragPlacements();

    auto clipIds = appStatus->selectedClips.get();
    if (clipIds.isEmpty())
        clipIds.append(clip.id);
    m_dragSessionId =
        editSessionManager->beginTransaction(AppStatus::EditObjectType::Clip, clip.id, clipIds);
    appStatus->currentEditObject = AppStatus::EditObjectType::Clip;
}

void TracksRhiWidget::updateDrag(const QPointF &position, const Qt::KeyboardModifiers modifiers) {
    if (!m_dragSession.active())
        return;
    const auto scene = m_viewport.viewportToScene(position);
    // A pointer above or below the lanes keeps the last row it was over, so the block stops at the
    // track range instead of jumping to the opposite end.
    const auto trackIndex = trackIndexAt(position);
    if (trackIndex >= 0)
        m_dragTrackIndex = trackIndex;
    if (!m_dragSession.update(m_viewport.sceneXToTick(scene.x()), m_dragTrackIndex,
                              modifiers == Qt::AltModifier))
        return;
    applyDragPlacements(m_dragSession.placements());
    m_dragMoved = m_dragSession.moved();
    scheduleSnapshot();
}

QList<int> TracksRhiWidget::orderedClipIds() const {
    QList<int> ids;
    for (const auto *track : appModel->tracks())
        for (const auto *clip : track->clips())
            ids.append(clip->id());
    return ids;
}

void TracksRhiWidget::captureSelectionStates(QList<ClipSelectionDrag::ClipState> &states) const {
    const auto &tracks = appModel->tracks();
    for (const auto *track : tracks) {
        const auto trackIndex = static_cast<int>(tracks.indexOf(track));
        for (const auto *clip : track->clips()) {
            if (!appStatus->selectedClips.get().contains(clip->id()))
                continue;

            ClipSelectionDrag::ClipState state;
            state.clipId = clip->id();
            state.trackIndex = trackIndex;
            state.properties = Clip::ClipCommonProperties(*clip);
            if (const auto *singing = qobject_cast<const SingingClip *>(clip)) {
                state.lengthResizable = true;
                state.contentLength = ClipResizeUtils::furthestContentEnd(
                    singing->notes().begin(), singing->notes().end(), AppGlobal::ticksPerWholeNote,
                    [](const Note *note) { return note->localStart() + note->length(); });
            }
            if (const auto *audio = qobject_cast<const AudioClip *>(clip);
                audio && audio->hasRealTimeAnchor()) {
                // The planner re-projects the millisecond truth, so it has to carry it.
                state.realTimeAnchored = true;
                state.properties.trimStartMs = audio->trimStartMs();
                state.properties.playLengthMs = audio->playLengthMs();
                state.properties.materialLengthMs = audio->materialLengthMs();
            }
            states.append(state);
        }
    }
}

void TracksRhiWidget::applyDragPlacements(const QList<ClipSelectionDrag::Placement> &placements) {
    m_placements.clear();
    for (const auto &placement : placements)
        m_placements.insert(placement.clipId, placement);
    for (const auto &placement : placements) {
        if (placement.clipId != m_dragAnchorId)
            continue;
        previewAnchorTrackColor(placement.trackIndex);
        break;
    }
}

void TracksRhiWidget::clearDragPlacements() {
    m_placements.clear();
}

void TracksRhiWidget::previewAnchorTrackColor(const int trackIndex) const {
    if (trackIndex < 0 || trackIndex >= appModel->tracks().size())
        return;
    editorViewController->previewActiveClipTrackColor(
        appModel->tracks().at(trackIndex)->colorIndex());
}

void TracksRhiWidget::restoreAnchorTrackColor() const {
    for (const auto &original : m_dragSession.originals()) {
        if (original.clipId != m_dragAnchorId)
            continue;
        previewAnchorTrackColor(original.trackIndex);
        return;
    }
}

int TracksRhiWidget::nudgeStep() const {
    const auto &selected = appStatus->selectedClips.get();
    auto tick = 0;
    if (!selected.isEmpty()) {
        if (const auto *clip = appModel->findClipById(selected.constFirst()))
            tick = clip->start() + clip->clipStart();
    }
    return snapStep(tick);
}

void TracksRhiWidget::nudgeSelection(const int deltaTicks, const int deltaTracks) const {
    if (deltaTicks == 0 && deltaTracks == 0)
        return;
    trackController->nudgeSelectedClips(deltaTicks, deltaTracks);
}

void TracksRhiWidget::commitDrag() {
    const auto committed = m_dragSession.active() && m_dragMoved;
    if (committed) {
        const auto placements = m_dragSession.placements();
        QList<ClipGeometryEdit> edits;
        edits.reserve(placements.size());
        for (const auto &placement : placements)
            edits.append({placement.clipId, placement.trackIndex, placement.properties});
        if (!edits.isEmpty()) {
            if (m_dragSession.gesture() == ClipSelectionDrag::Gesture::Move)
                trackController->onMoveClips(edits);
            else
                trackController->onResizeClips(edits);
        }
        // The committed track colour stays on the bottom panel: the active clip now lives there.
    } else {
        restoreAnchorTrackColor();
    }
    finishDragSession(committed ? EditSessionEndReason::Commit : EditSessionEndReason::Discard);
    // A plain click on a clip that was already part of the selection collapses it to that clip,
    // matching how notes and parameter anchors behave.
    const auto collapsed = m_selectionModel.release(appStatus->selectedClips.get(), committed);
    if (collapsed != appStatus->selectedClips.get())
        syncSelection(collapsed);
    clearDragPlacements();
    m_dragSession.reset();
    m_dragMode = DragMode::None;
    m_dragMoved = false;
    m_dragAnchorId = -1;
    m_dragTrackIndex = -1;
    disarmDragAutoScroll();
    setSceneLengthExtension(0);
}

void TracksRhiWidget::discardDrag() {
    restoreAnchorTrackColor();
    finishDragSession(EditSessionEndReason::Discard);
    m_selectionModel.cancelPress();
    clearDragPlacements();
    m_dragSession.reset();
    m_dragMode = DragMode::None;
    m_dragMoved = false;
    m_dragAnchorId = -1;
    m_dragTrackIndex = -1;
    disarmDragAutoScroll();
    setSceneLengthExtension(0);
    scheduleSnapshot();
}

void TracksRhiWidget::finishDragSession(const EditSessionEndReason reason) {
    if (m_dragSessionId != 0 && editSessionManager->hasActiveTransaction() &&
        editSessionManager->activeSession().sessionId == m_dragSessionId) {
        editSessionManager->endTransaction(m_dragSessionId, reason);
    }
    m_dragSessionId = 0;
    if (!editSessionManager->hasActiveTransaction())
        appStatus->currentEditObject = AppStatus::EditObjectType::None;
}

bool TracksRhiWidget::updateClipSelection(const ClipSnapshot &clip,
                                          const Qt::KeyboardModifiers modifiers) {
    const auto result = m_selectionModel.press(appStatus->selectedClips.get(), orderedClipIds(),
                                               clip.id, modifiers);
    const auto targetSelected = result.targetSelected;
    syncSelection(result.selection, targetSelected ? clip.trackIndex : -1);
    if (!targetSelected)
        return false;
    trackController->setActiveClip(clip.id);
    return true;
}

void TracksRhiWidget::syncSelection(const QList<int> &ids, const int preferredTrack) {
    trackController->setSelectedClips(ids);
    // Keep the range anchor in step with the platform selection. Clearing the selection through the
    // blank canvas has to drop the anchor too, otherwise a later Shift click ranges from a clip
    // that is not selected any more.
    m_selectionModel.synchronize(ids);
    auto trackIndex = preferredTrack;
    if (trackIndex < 0 && !ids.isEmpty()) {
        Track *track = nullptr;
        if (appModel->findClipById(ids.constFirst(), track) && track)
            trackIndex = appModel->tracks().indexOf(track);
    }
    if (trackIndex >= 0)
        trackController->setSelectedTrackIndex(trackIndex);
}

void TracksRhiWidget::updateCursor(const QPointF &position) {
    const auto *clip = hitTest(position);
    if (!clip) {
        unsetCursor();
        return;
    }
    const auto physical = m_viewport.viewportToScene(position).x() * devicePixelRatioF();
    const auto relative = physical - clip->modelPhysicalRect.left();
    const auto tolerance = EditorPointer::resizeTolerance() * devicePixelRatioF();
    const auto edge =
        EditorResizeUtils::horizontalEdgeAt(relative, clip->modelPhysicalRect.width(), tolerance);
    setCursor(edge == EditorResizeUtils::HorizontalEdge::None ? Qt::ArrowCursor
                                                              : Qt::SizeHorCursor);
}

void TracksRhiWidget::setAutoPageTurn(const bool enabled) {
    m_autoTurnPage = enabled;
    if (enabled)
        handleAutoPageTurn();
}

void TracksRhiWidget::handleAutoPageTurn() {
    if (!m_autoTurnPage || !m_autoPageTurnAvailable || m_edgeAutoScroller.isRunning() ||
        appStatus->currentEditObject != AppStatus::EditObjectType::None) {
        return;
    }

    const auto start = startTick();
    const auto end = endTick();
    const auto range = end - start;
    if (range <= 0.0)
        return;
    if (m_playbackPosition > end) {
        m_wheelController->stop();
        m_viewport.setStartTick(m_playbackPosition > end + range ? m_playbackPosition : end);
    } else if (m_playbackPosition < start) {
        m_wheelController->stop();
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
    const auto placement = m_placements.constFind(clip->id());
    if (placement != m_placements.cend())
        return placement->properties;
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
    updatePlaybackOverlay();
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
