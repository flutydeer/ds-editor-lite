#include "TracksGraphicsView.h"

#include "TracksGraphicsScene.h"
#include "Controller/EditorViewController.h"
#include "UI/Views/Common/EditorSelectionUtils.h"
#include "Controller/PlaybackController.h"
#include "Controller/TrackController.h"
#include "Global/TracksEditorGlobal.h"
#include "GraphicsItem/AbstractClipView.h"
#include "GraphicsItem/AudioClipView.h"
#include "GraphicsItem/SingingClipView.h"
#include "GraphicsItem/TrackEditorBackgroundView.h"
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/EditSessionManager.h"
#include <lite/GUI/Controls/AccentButton.h>
#include "UI/Utils/SpeakerMixDisplayUtils.h"
#include "Utils/UiLanguageManager.h"
#include "UI/Views/Common/EditorPointerUtils.h"
#include "UI/Views/Common/EditorResizeUtils.h"
#include <lite/MusicBase/TimelineSnapUtils.h>

#include <algorithm>
#include <cmath>
#include <numbers>

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QScrollBar>

TracksGraphicsView::TracksGraphicsView(TracksGraphicsScene *scene, QWidget *parent)
    : TimeGraphicsView(scene, true, parent), m_scene(scene) {
    setAttribute(Qt::WA_StyledBackground);
    setObjectName("TracksGraphicsView");
    setScaleYMin(0.575);
    setEnsureSceneFillViewY(false);
    setScaleXMax(10000);
    setPixelsPerQuarterNote(TracksEditorGlobal::pixelsPerQuarterNote);
    setDragBehavior(DragBehavior::RectSelect);
    setMinimumHeight(0);
    // Arrow keys nudge the clip selection, so the canvas has to keep the keyboard after a click.
    setFocusPolicy(Qt::StrongFocus);
    // Accept external file drags (Phase 1 drop slot resolution)
    viewport()->setAcceptDrops(true);

    connect(this, &TimeGraphicsView::scaleChanged, this, [this](double, double) {
        // Keep the drop overlay geometry in sync while the canvas is zoomed
        updateDropOverlayGeometry();
    });

    connect(appStatus, &AppStatus::activeClipIdChanged, this, [this](const int clipId) {
        if (clipId == -1) {
            resetActiveClips();
            return;
        }

        if (const auto clipItem = findClipById(clipId)) {
            resetActiveClips();
            clipItem->setActiveClip(true);
        } else
            qFatal() << "Clip not found: " << clipId;
    });
}

void TracksGraphicsView::setSnapGrid(TrackEditorBackgroundView *grid) {
    m_snapGrid = grid;
    if (m_snapGrid)
        m_snapGrid->setSelectedTrackColor(m_selectedTrackColor);
}

QColor TracksGraphicsView::selectedTrackColor() const {
    return m_selectedTrackColor;
}

void TracksGraphicsView::setSelectedTrackColor(const QColor &color) {
    if (m_selectedTrackColor == color)
        return;
    m_selectedTrackColor = color;
    if (m_snapGrid)
        m_snapGrid->setSelectedTrackColor(color);
}

QColor TracksGraphicsView::clipSelectedBorderColor() const {
    return AbstractClipView::selectedBorderColor();
}

void TracksGraphicsView::setClipSelectedBorderColor(const QColor &color) {
    if (AbstractClipView::selectedBorderColor() == color)
        return;
    AbstractClipView::setSelectedBorderColor(color);
    for (const auto item : m_scene->items())
        if (const auto clip = dynamic_cast<AbstractClipView *>(item))
            clip->update();
}

void TracksGraphicsView::setClipSelection(const QList<int> &selected) {
    applyClipSelection(selected);
    // Keep the range anchor in step with the platform selection. Clearing the selection through the
    // blank canvas has to drop the anchor too, otherwise a later Shift click ranges from a clip
    // that is not selected any more.
    m_selectionModel.synchronize(selected);
}

int TracksGraphicsView::snapStep(const bool snapOff, const int atTick) const {
    if (snapOff)
        return 1;
    return m_snapGrid ? m_snapGrid->logicalGridStepForCurrentScale(atTick)
                      : TimelineSnapUtils::quantizeToTicks(128);
}

QList<int> TracksGraphicsView::selectedClipsId() const {
    QList<int> result;
    for (const auto clipItem : selectedClipItems())
        result.append(clipItem->id());
    return result;
}

void TracksGraphicsView::onNewSingingClip() const {
    trackController->onNewSingingClip(m_trackIndex, m_tick);
}

bool TracksGraphicsView::event(QEvent *event) {
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride) {
        const auto key = dynamic_cast<QKeyEvent *>(event)->key();
        if (key == Qt::Key_Escape) {
            discardAction();
        }
    } else if (event->type() == QEvent::WindowDeactivate) {
        discardAction();
    }
    return TimeGraphicsView::event(event);
}

bool TracksGraphicsView::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::Leave)
        clearTrackPastePreview();
    return TimeGraphicsView::eventFilter(watched, event);
}

void TracksGraphicsView::mousePressEvent(QMouseEvent *event) {
    cancelRequested = false;

    if (const auto item = itemAt(event->pos())) {
        if (const auto clipItem = dynamic_cast<AbstractClipView *>(item)) {
            if (event->button() != Qt::LeftButton) {
                updateClipSelection(clipItem, event->modifiers());
                setCursor(Qt::ArrowCursor);
            } else {
                // Arrow-key nudging needs the canvas to own the keyboard, the same way the RHI
                // arrangement takes focus on press.
                setFocus(Qt::MouseFocusReason);
                if (updateClipSelection(clipItem, event->modifiers()))
                    prepareForMovingOrResizingClip(event, clipItem);
            }
        } else {
            m_selectionModel.cancelPress();
            clearSelections();
            TimeGraphicsView::mousePressEvent(event);
        }
    } else {
        m_selectionModel.cancelPress();
        TimeGraphicsView::mousePressEvent(event);
    }
    syncClipSelectionToAppStatus();
    event->ignore();
}

void TracksGraphicsView::mouseMoveEvent(QMouseEvent *event) {
    if (cancelRequested) {
        TimeGraphicsView::mouseMoveEvent(event);
        return;
    }

    updateClipDragAt(event->pos(), event->modifiers());
    TimeGraphicsView::mouseMoveEvent(event);
}

void TracksGraphicsView::updateClipDragAt(const QPoint &viewportPos,
                                          const Qt::KeyboardModifiers modifiers) {
    if (!m_dragSession.active())
        return;

    const auto scenePos = mapToScene(viewportPos);
    // A pointer above or below the lanes keeps the last row it was over, so the block stops at the
    // track range instead of jumping to the opposite end.
    const auto trackIndex = m_scene->trackIndexAt(scenePos.y());
    if (trackIndex >= 0)
        m_dragTrackIndex = trackIndex;
    if (!m_dragSession.update(tickAtSceneX(scenePos.x()), m_dragTrackIndex,
                              modifiers == Qt::AltModifier))
        return;

    applyDragPlacements(m_dragSession.placements());
    m_movedBeforeMouseUp = m_dragSession.moved();
}

void TracksGraphicsView::onEdgeAutoScrollFrame(const QPoint &clampedViewportPos,
                                               const Qt::KeyboardModifiers modifiers) {
    if (cancelRequested)
        return;

    if (m_externalDragActive) {
        // Keep the drop slot and overlay in sync while edge scrolling
        updateExternalDropOverlay(clampedViewportPos);
        return;
    }

    if (m_dragSession.active()) {
        // Extend the scene temporarily when dragging against the right edge so
        // the block can travel past the current project tail.
        if (m_dragSession.gesture() == ClipSelectionDrag::Gesture::Move ||
            m_dragSession.gesture() == ClipSelectionDrag::Gesture::ResizeRight) {
            const auto hBar = horizontalScrollBar();
            if (hBar->value() >= hBar->maximum()) {
                const auto visibleTickSpan = qRound(endTick() - startTick());
                setSceneLengthExtension(sceneLengthExtension() + visibleTickSpan);
            }
        }
        updateClipDragAt(clampedViewportPos, modifiers);
        return;
    }
    // Rubber band selection is handled by the base class
    TimeGraphicsView::onEdgeAutoScrollFrame(clampedViewportPos, modifiers);
}

void TracksGraphicsView::mouseReleaseEvent(QMouseEvent *event) {
    const auto dragActive = m_dragSession.active();
    const auto committed = dragActive && m_movedBeforeMouseUp && !cancelRequested;
    if (committed)
        commitAction();
    else {
        if (event->button() == Qt::LeftButton && !cancelRequested) {
            // A plain click on a clip that was already part of the selection collapses it to that
            // clip, matching how notes and parameter anchors behave.
            const auto collapsed =
                m_selectionModel.release(selectedClipsId(), dragActive && m_movedBeforeMouseUp);
            if (collapsed != selectedClipsId())
                applyClipSelection(collapsed);
        } else {
            m_selectionModel.cancelPress();
        }
        editSessionManager->endActiveTransaction(EditSessionEndReason::Discard);
        resetEditState();
    }
    cancelRequested = false;
    syncClipSelectionToAppStatus();
    TimeGraphicsView::mouseReleaseEvent(event);
}

void TracksGraphicsView::mouseDoubleClickEvent(QMouseEvent *event) {
    const auto scenePos = mapToScene(event->position().toPoint());
    m_trackIndex = m_scene->trackIndexAt(scenePos.y());
    if (m_trackIndex == -1)
        return;

    const auto tick = m_scene->tickAt(scenePos.x());
    if (const auto item = itemAt(event->pos())) {
        if (auto clipItem = dynamic_cast<AbstractClipView *>(item)) {
            editorViewController->showBottomPanelPage(QStringLiteral("ClipEditor"));
            double tick = playbackController->position();
            double keyIndex = 60;
            // 音符绘制区内双击：钢琴卷帘居中到双击落点（tick + keyIndex）
            if (const auto singingClip = dynamic_cast<SingingClipView *>(clipItem)) {
                const double key = singingClip->keyIndexAtScenePos(scenePos);
                if (key >= 0) {
                    tick = m_scene->tickAt(scenePos.x());
                    keyIndex = key;
                }
            }
            editorViewController->centerPianoRollAt(tick, keyIndex);
        } else if (dynamic_cast<TrackEditorBackgroundView *>(item)) {
            m_tick = TimelineSnapUtils::snapDown(tick, snapStep(false, tick), appModel->timeline());
            onNewSingingClip();
        }
    }

    TimeGraphicsView::mouseDoubleClickEvent(event);
}

EditorTouchTarget::BlankDragAction TracksGraphicsView::touchBlankDragAction() const {
    // Empty canvas in the arrangement view carries nothing to create, so a
    // plain finger drag there scrolls. Rubber band selection stays reachable
    // through a long press.
    return BlankDragAction::Pan;
}

void TracksGraphicsView::cancelTouchPointerInteraction() {
    discardAction();
    TimeGraphicsView::cancelTouchPointerInteraction();
}

void TracksGraphicsView::contextMenuEvent(QContextMenuEvent *event) {
    const auto scenePos = mapToScene(event->pos());
    const auto trackIndex = m_scene->trackIndexAt(scenePos.y());
    if (trackIndex == -1)
        return;

    const auto tick = m_scene->tickAt(scenePos.x());
    const auto *item = itemAt(event->pos());
    TrackEditorMenuContext context;
    context.globalPos = event->globalPos();
    context.rawTick = tick;
    context.snappedTick = TimelineSnapUtils::snapDown(
        context.rawTick, snapStep(false, context.rawTick), appModel->timeline());
    context.trackIndex = trackIndex;
    if (dynamic_cast<const TrackEditorBackgroundView *>(item)) {
        clearSelections();
        context.target = TrackEditorMenuContext::Target::Background;
    } else if (const auto *clip = dynamic_cast<const AbstractClipView *>(item)) {
        context.clipId = clip->id();
        context.selectedClipIds = selectedClipsId();
        context.target = clip->clipType() == IClip::Audio
                             ? TrackEditorMenuContext::Target::AudioClip
                             : TrackEditorMenuContext::Target::SingingClip;
        if (const auto *audio = qobject_cast<const AudioClip *>(appModel->findClipById(clip->id())))
            context.audioMissing = audio->pathStatus() == AudioClip::PathStatus::Missing;
    } else {
        TimeGraphicsView::contextMenuEvent(event);
        return;
    }
    emit contextMenuRequested(context);
    event->accept();
}

void TracksGraphicsView::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        m_externalDragActive = true;
        updateExternalDropOverlay(event->position().toPoint());
        // Arm edge auto scroll so the user can drag past the viewport edges
        armEdgeAutoScroll(Qt::Vertical);
        return;
    }
    TimeGraphicsView::dragEnterEvent(event);
}

void TracksGraphicsView::dragMoveEvent(QDragMoveEvent *event) {
    if (m_externalDragActive && event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        updateExternalDropOverlay(event->position().toPoint());
        // Re-arm on every move: refresh the hot zone state while keeping the
        // press position recorded at drag enter.
        armEdgeAutoScroll(Qt::Vertical);
        return;
    }
    TimeGraphicsView::dragMoveEvent(event);
}

void TracksGraphicsView::dragLeaveEvent(QDragLeaveEvent *event) {
    if (m_externalDragActive) {
        endExternalDropOverlay();
        event->accept();
        return;
    }
    TimeGraphicsView::dragLeaveEvent(event);
}

void TracksGraphicsView::dropEvent(QDropEvent *event) {
    if (m_externalDragActive && event->mimeData()->hasUrls()) {
        if (const auto slot = dropSlotAt(event->position().toPoint()))
            emit externalDropRequested(*slot, event->mimeData()->urls());
        endExternalDropOverlay();
        event->acceptProposedAction();
        return;
    }
    QGraphicsView::dropEvent(event);
}

std::optional<TrackDropSlot> TracksGraphicsView::dropSlotAt(const QPoint &viewportPos) const {
    const auto scenePos = mapToScene(viewportPos);
    if (scenePos.y() < 0)
        return std::nullopt;
    TrackDropSlot slot;
    const auto rawTick = m_scene->tickAt(scenePos.x());
    slot.snappedTick =
        TimelineSnapUtils::snapNearest(rawTick, snapStep(false, rawTick), appModel->timeline());
    const auto trackIndex = m_scene->trackIndexAt(scenePos.y());
    if (trackIndex >= 0) {
        slot.kind = TrackDropSlot::Kind::ExistingTrack;
        slot.trackIndex = trackIndex;
        return slot;
    }
    if (m_scene->isAppendSlotAt(scenePos.y())) {
        slot.kind = TrackDropSlot::Kind::Append;
        slot.trackIndex = appModel->tracks().size();
        return slot;
    }
    return std::nullopt;
}

void TracksGraphicsView::updateExternalDropOverlay(const QPoint &viewportPos) {
    m_dropSlot = dropSlotAt(viewportPos);
    ensureDropOverlayItems();
    updateDropOverlayGeometry();
}

void TracksGraphicsView::endExternalDropOverlay() {
    m_externalDragActive = false;
    m_dropSlot.reset();
    disarmEdgeAutoScroll();
    if (m_dropHighlightItem)
        m_dropHighlightItem->hide();
    if (m_dropIndicatorLine)
        m_dropIndicatorLine->hide();
}

void TracksGraphicsView::ensureDropOverlayItems() {
    if (m_dropHighlightItem && m_dropIndicatorLine)
        return;
    auto highlightPen = QPen(m_dropHighlightColor);
    highlightPen.setWidthF(1);
    m_dropHighlightItem = m_scene->addRect(QRectF(), highlightPen, QBrush(m_dropHighlightColor));
    m_dropHighlightItem->setZValue(10000);
    m_dropHighlightItem->setAcceptedMouseButtons(Qt::NoButton);
    m_dropHighlightItem->hide();
    m_dropIndicatorLine = m_scene->addLine(QLineF(), QPen(m_dropIndicatorColor, 1));
    m_dropIndicatorLine->setZValue(10001);
    m_dropIndicatorLine->setAcceptedMouseButtons(Qt::NoButton);
    m_dropIndicatorLine->hide();
}

QColor TracksGraphicsView::dropHighlightColor() const {
    return m_dropHighlightColor;
}

void TracksGraphicsView::setDropHighlightColor(const QColor &color) {
    if (m_dropHighlightColor == color)
        return;
    m_dropHighlightColor = color;
    if (m_dropHighlightItem) {
        auto pen = QPen(color);
        pen.setWidthF(1);
        m_dropHighlightItem->setPen(pen);
        m_dropHighlightItem->setBrush(QBrush(color));
    }
}

QColor TracksGraphicsView::dropIndicatorColor() const {
    return m_dropIndicatorColor;
}

void TracksGraphicsView::setDropIndicatorColor(const QColor &color) {
    if (m_dropIndicatorColor == color)
        return;
    m_dropIndicatorColor = color;
    if (m_dropIndicatorLine)
        m_dropIndicatorLine->setPen(QPen(color, 1));
}

void TracksGraphicsView::updateDropOverlayGeometry() {
    if (!m_dropHighlightItem || !m_dropIndicatorLine)
        return;
    if (!m_externalDragActive || !m_dropSlot) {
        m_dropHighlightItem->hide();
        m_dropIndicatorLine->hide();
        return;
    }
    const auto slot = *m_dropSlot;
    const auto trackHeightPx = TracksEditorGlobal::trackHeight * scaleY();
    const auto sceneWidth = m_scene->sceneRect().width();
    m_dropHighlightItem->setRect(0, slot.trackIndex * trackHeightPx, sceneWidth, trackHeightPx);
    m_dropHighlightItem->show();
    const auto contentHeight = (appModel->tracks().size() + 1) * trackHeightPx;
    const auto x = tickToSceneX(slot.snappedTick);
    m_dropIndicatorLine->setLine(x, 0, x, contentHeight);
    m_dropIndicatorLine->show();
}

void TracksGraphicsView::discardAction() {
    cancelRequested = true;
    restoreAnchorTrackColor();
    restoreDragOriginals();
    editSessionManager->endActiveTransaction(EditSessionEndReason::Discard);
    resetEditState();
}

void TracksGraphicsView::commitAction() {
    if (m_dragSession.active() && m_movedBeforeMouseUp) {
        const auto placements = m_dragSession.placements();
        QList<ClipGeometryEdit> edits;
        edits.reserve(placements.size());
        QSet<int> committedIds;
        for (const auto &placement : placements) {
            edits.append({placement.clipId, placement.trackIndex, placement.properties});
            committedIds.insert(placement.clipId);
        }
        // A clip the planner dropped (its target was illegal) still shows a preview, so put it
        // back before the command runs; the model signal only refreshes the clips that did change.
        restoreDragOriginals(committedIds);
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
    editSessionManager->endActiveTransaction(EditSessionEndReason::Commit);
    resetEditState();
}

void TracksGraphicsView::resetEditState() {
    m_movedBeforeMouseUp = false;
    m_dragSession.reset();
    m_dragAnchorId = -1;
    m_dragTrackIndex = -1;
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
    disarmEdgeAutoScroll();
    // Drop the temporary drag extension; the scene falls back to the latest
    // externally driven base length.
    setSceneLengthExtension(0);
}

void TracksGraphicsView::clearTrackPastePreview() {
    for (auto view : m_pastePreviewClipViews) {
        m_scene->removeCommonItem(view);
        delete view;
    }
    m_pastePreviewClipViews.clear();
}

void TracksGraphicsView::showTrackSplitPreview(const TrackSplitPreviewData &data) {
    if (data.trackIndex < 0)
        return;
    if (!m_splitPreviewItem) {
        m_splitPreviewItem = m_scene->addPath(QPainterPath(), QPen(QColor(255, 100, 100), 2));
        m_splitPreviewItem->setZValue(10002);
        m_splitPreviewItem->setAcceptedMouseButtons(Qt::NoButton);
        m_splitPreviewItem->hide();
    }

    // Same forked-line geometry as the piano roll's split tool indicator
    constexpr double extensionLength = 8.0;
    constexpr double forkLength = 6.0;
    constexpr double forkAngle = 45.0;
    const auto x = tickToSceneX(data.splitTick);
    const auto trackHeightPx = TracksEditorGlobal::trackHeight * scaleY();
    const auto lineTop = data.trackIndex * trackHeightPx - extensionLength;
    const auto lineBottom = (data.trackIndex + 1) * trackHeightPx + extensionLength;
    const auto forkAngleRad = forkAngle * std::numbers::pi / 180.0;
    const auto forkOffsetX = forkLength * std::sin(forkAngleRad);
    const auto forkOffsetY = forkLength * std::cos(forkAngleRad);

    QPainterPath path;
    path.moveTo(x, lineTop);
    path.lineTo(x, lineBottom);
    path.moveTo(x, lineTop);
    path.lineTo(x - forkOffsetX, lineTop - forkOffsetY);
    path.moveTo(x, lineTop);
    path.lineTo(x + forkOffsetX, lineTop - forkOffsetY);
    path.moveTo(x, lineBottom);
    path.lineTo(x - forkOffsetX, lineBottom + forkOffsetY);
    path.moveTo(x, lineBottom);
    path.lineTo(x + forkOffsetX, lineBottom + forkOffsetY);
    m_splitPreviewItem->setPath(path);
    m_splitPreviewItem->show();
}

void TracksGraphicsView::clearTrackSplitPreview() {
    if (m_splitPreviewItem)
        m_splitPreviewItem->hide();
}

void TracksGraphicsView::showTrackPastePreview(const TrackPastePreviewData &data,
                                               const int previewTick, const int baseTrackIndex) {
    if (!m_pastePreviewClipViews.isEmpty() || data.clips.isEmpty() || appModel->tracks().isEmpty())
        return;

    auto firstClipStart = data.clips.first().properties.start;
    for (const auto &clip : data.clips)
        firstClipStart = std::min(firstClipStart, clip.properties.start);

    for (const auto &clip : data.clips) {
        const auto targetTrack = std::clamp(baseTrackIndex + clip.trackIndexOffset, 0,
                                            static_cast<int>(appModel->tracks().size()) - 1);
        const auto *track = appModel->tracks().at(targetTrack);
        const auto targetStart = previewTick + clip.properties.start - firstClipStart;
        AbstractClipView *clipView = nullptr;
        if (clip.type == IClip::Singing) {
            auto *view = new SingingClipView(-1);
            view->loadCommonProperties(clip.properties);
            view->setTrackIndex(targetTrack);
            view->setStart(targetStart);
            QVector<std::tuple<int, int, int>> notes;
            notes.reserve(clip.notes.size());
            for (const auto &note : clip.notes)
                notes.append({note.start, note.length, note.key});
            view->loadPreviewNotes(notes);
            view->setSingerName(
                track->singerInfo().displayName(UiLanguageManager::currentBcp47Candidates()));
            view->setSpeakerName(SpeakerMixDisplayUtils::speakerDisplayName(
                track->singerInfo(), track->speakerInfo(), track->speakerMixData()));
            view->setDefaultLanguage(clip.defaultLanguage);
            clipView = view;
        } else if (clip.type == IClip::Audio) {
            auto *view = new AudioClipView(-1);
            view->loadCommonProperties(clip.properties);
            view->setTrackIndex(targetTrack);
            view->setStart(targetStart);
            view->setPath(clip.audioPath);
            view->setTimeline(appModel->timeline());
            view->setAudioInfo(clip.audioInfo);
            // No decoding controller is behind a preview view, and its samples come straight from
            // the model, so it is loaded by construction rather than by a task finishing.
            view->setLoadState(AppGlobal::AudioClipLoadState::Loaded);
            clipView = view;
        }

        if (!clipView)
            continue;
        clipView->setColorIndex(track->colorIndex());
        clipView->setOpacity(0.35);
        clipView->setAcceptedMouseButtons(Qt::NoButton);
        clipView->setAcceptHoverEvents(false);
        clipView->setFlag(QGraphicsItem::ItemIsSelectable, false);
        m_scene->addCommonItem(clipView);
        m_pastePreviewClipViews.append(clipView);
    }
}

void TracksGraphicsView::syncClipSelectionToAppStatus() const {
    const auto ids = selectedClipsId();
    trackController->setSelectedClips(ids);
    if (!ids.isEmpty()) {
        Track *track;
        appModel->findClipById(ids.first(), track);
        const auto trackIndex = appModel->tracks().indexOf(track);
        if (trackIndex >= 0)
            trackController->setSelectedTrackIndex(trackIndex);
    }
}

void TracksGraphicsView::prepareForMovingOrResizingClip(const QMouseEvent *event,
                                                        AbstractClipView *clipItem) {
    const auto scenePos = mapToScene(event->pos());
    const auto rPos = clipItem->mapFromScene(scenePos);
    const auto edge = EditorResizeUtils::horizontalEdgeAt(rPos.x(), clipItem->rect().width(),
                                                          EditorPointer::resizeTolerance());

    auto gesture = ClipSelectionDrag::Gesture::Move;
    if (edge == EditorResizeUtils::HorizontalEdge::Left)
        gesture = ClipSelectionDrag::Gesture::ResizeLeft;
    else if (edge == EditorResizeUtils::HorizontalEdge::Right)
        gesture = ClipSelectionDrag::Gesture::ResizeRight;

    m_dragAnchorId = clipItem->id();
    QList<ClipSelectionDrag::ClipState> states;
    captureSelectionStates(states);
    const auto pointerTrackIndex = m_scene->trackIndexAt(scenePos.y());
    m_dragTrackIndex = pointerTrackIndex;

    ClipSelectionDrag::Session::Environment environment;
    environment.trackCount = static_cast<int>(appModel->tracks().size());
    environment.timeline = &appModel->timeline();
    environment.gridStepAt = [this](const int tick) { return snapStep(false, tick); };
    m_dragSession.begin(gesture, m_dragAnchorId, states, environment,
                        static_cast<double>(tickAtSceneX(scenePos.x())), pointerTrackIndex);
    m_movedBeforeMouseUp = false;

    QList<int> clipIds;
    for (const auto clip : selectedClipItems())
        clipIds.append(clip->id());
    if (clipIds.isEmpty())
        clipIds.append(clipItem->id());
    editSessionManager->beginTransaction(AppStatus::EditObjectType::Clip, clipItem->id(), clipIds);
    appStatus->currentEditObject = AppStatus::EditObjectType::Clip;

    // Moving allows both axes (track change); resizing is horizontal only
    armEdgeAutoScroll(gesture == ClipSelectionDrag::Gesture::Move
                          ? (Qt::Horizontal | Qt::Vertical)
                          : Qt::Orientations(Qt::Horizontal),
                      event->pos());
}

QList<int> TracksGraphicsView::orderedClipIds() const {
    QList<int> ids;
    for (const auto *track : appModel->tracks())
        for (const auto *clip : track->clips())
            ids.append(clip->id());
    return ids;
}

void TracksGraphicsView::captureSelectionStates(QList<ClipSelectionDrag::ClipState> &states) const {
    const auto &tracks = appModel->tracks();
    for (const auto *track : tracks) {
        const auto trackIndex = static_cast<int>(tracks.indexOf(track));
        for (const auto *clip : track->clips()) {
            auto *item = findClipById(clip->id());
            if (!item || !item->isSelected())
                continue;

            ClipSelectionDrag::ClipState state;
            state.clipId = clip->id();
            state.trackIndex = trackIndex;
            state.properties = Clip::ClipCommonProperties(*clip);
            state.lengthResizable = item->canResizeLength();
            state.contentLength = item->contentLength();
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

void TracksGraphicsView::applyDragPlacements(
    const QList<ClipSelectionDrag::Placement> &placements) const {
    const auto &tracks = appModel->tracks();
    QSet<int> applied;
    for (const auto &placement : placements) {
        auto *item = findClipById(placement.clipId);
        if (!item)
            continue;
        item->loadCommonProperties(placement.properties);
        applied.insert(placement.clipId);
        if (placement.trackIndex < 0 || placement.trackIndex >= tracks.size())
            continue;
        item->setTrackIndex(placement.trackIndex);
        item->setColorIndex(tracks.at(placement.trackIndex)->colorIndex());
    }
    // The planner drops a clip whose target would be illegal. It still has to lose the preview a
    // previous move left on it.
    restoreDragOriginals(applied);
    for (const auto &placement : placements) {
        if (placement.clipId != m_dragAnchorId)
            continue;
        previewAnchorTrackColor(placement.trackIndex);
        break;
    }
}

void TracksGraphicsView::restoreDragOriginals(const QSet<int> &skip) const {
    const auto &tracks = appModel->tracks();
    for (const auto &original : m_dragSession.originals()) {
        if (skip.contains(original.clipId))
            continue;
        auto *item = findClipById(original.clipId);
        if (!item)
            continue;
        item->loadCommonProperties(original.properties);
        item->setTrackIndex(original.trackIndex);
        if (original.trackIndex >= 0 && original.trackIndex < tracks.size())
            item->setColorIndex(tracks.at(original.trackIndex)->colorIndex());
    }
}

void TracksGraphicsView::previewAnchorTrackColor(const int trackIndex) const {
    if (trackIndex < 0 || trackIndex >= appModel->tracks().size())
        return;
    editorViewController->previewActiveClipTrackColor(
        appModel->tracks().at(trackIndex)->colorIndex());
}

void TracksGraphicsView::restoreAnchorTrackColor() const {
    for (const auto &original : m_dragSession.originals()) {
        if (original.clipId != m_dragAnchorId)
            continue;
        previewAnchorTrackColor(original.trackIndex);
        return;
    }
}

int TracksGraphicsView::tickAtSceneX(const double sceneX) const {
    return qRound((sceneX - m_scene->leftMarginPx()) / scaleX() /
                  TracksEditorGlobal::pixelsPerQuarterNote * AppGlobal::ticksPerQuarterNote);
}

int TracksGraphicsView::nudgeStep() const {
    const auto ids = selectedClipsId();
    auto tick = 0;
    if (!ids.isEmpty()) {
        if (const auto *clip = appModel->findClipById(ids.constFirst()))
            tick = clip->start() + clip->clipStart();
    }
    return snapStep(false, tick);
}

void TracksGraphicsView::nudgeSelection(const int deltaTicks, const int deltaTracks) const {
    if (deltaTicks == 0 && deltaTracks == 0)
        return;
    trackController->nudgeSelectedClips(deltaTicks, deltaTracks);
}

void TracksGraphicsView::keyPressEvent(QKeyEvent *event) {
    const auto hasSelection = !selectedClipsId().isEmpty();
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
    // Nothing to nudge means the arrows still belong to the inherited scrolling behaviour.
    TimeGraphicsView::keyPressEvent(event);
}

AbstractClipView *TracksGraphicsView::findClipById(const int id) const {
    for (const auto item : m_scene->items())
        if (const auto clip = dynamic_cast<AbstractClipView *>(item))
            if (clip->id() == id)
                return clip;
    return nullptr;
}

bool TracksGraphicsView::updateClipSelection(AbstractClipView *clipItem,
                                             const Qt::KeyboardModifiers modifiers) {
    const auto result =
        m_selectionModel.press(selectedClipsId(), orderedClipIds(), clipItem->id(), modifiers);
    applyClipSelection(result.selection);
    if (!result.targetSelected)
        return false;
    trackController->setActiveClip(clipItem->id());
    return true;
}

void TracksGraphicsView::applyClipSelection(const QList<int> &selected) const {
    bool changed = false;
    for (const auto item : m_scene->items()) {
        if (const auto clip = dynamic_cast<AbstractClipView *>(item)) {
            const auto shouldSelect = selected.contains(clip->id());
            changed |= clip->isSelected() != shouldSelect;
            clip->setSelected(shouldSelect);
        } else if (item->isSelected()) {
            changed = true;
            item->setSelected(false);
        }
    }
    if (changed)
        viewport()->update();
}

void TracksGraphicsView::clearSelections() const {
    applyClipSelection({});
    syncClipSelectionToAppStatus();
}

void TracksGraphicsView::resetActiveClips() const {
    for (const auto item : m_scene->items())
        if (const auto clip = dynamic_cast<AbstractClipView *>(item))
            if (clip->activeClip())
                clip->setActiveClip(false);
}

QList<AbstractClipView *> TracksGraphicsView::selectedClipItems() const {
    QList<AbstractClipView *> result;
    for (const auto item : m_scene->items())
        if (const auto clip = dynamic_cast<AbstractClipView *>(item))
            if (clip->isSelected())
                result.append(clip);
    return result;
}

void TracksGraphicsView::changeEvent(QEvent *event) {
    TimeGraphicsView::changeEvent(event);
}
