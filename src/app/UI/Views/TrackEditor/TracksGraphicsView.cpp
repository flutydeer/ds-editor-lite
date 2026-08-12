#include "TracksGraphicsView.h"

#include "AudioClipDragState.h"
#include "ClipResizeUtils.h"
#include "TracksGraphicsScene.h"
#include "Controller/EditorViewController.h"
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
#include "UI/Views/Common/EditorResizeUtils.h"
#include <lite/MusicBase/TimelineSnapUtils.h>

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QGraphicsLineItem>
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
            qDebug() << "TracksGraphicsView::mousePressEvent mouse down on clip";
            if (selectedClipItems().count() <= 1 || !selectedClipItems().contains(clipItem))
                clearSelections();
            clipItem->setSelected(true);
            trackController->setActiveClip(clipItem->id());
            if (event->button() != Qt::LeftButton) {
                m_mouseMoveBehavior = None;
                setCursor(Qt::ArrowCursor);
            } else {
                prepareForMovingOrResizingClip(event, clipItem);
            }
        } else {
            clearSelections();
            TimeGraphicsView::mousePressEvent(event);
        }
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
    if (m_mouseMoveBehavior == None || !m_currentEditingClip)
        return;

    m_tempQuantizeOff = modifiers == Qt::AltModifier;

    const auto curPos = mapToScene(viewportPos);
    const auto dx = (curPos.x() - m_mouseDownPos.x()) / scaleX() /
                    TracksEditorGlobal::pixelsPerQuarterNote * AppGlobal::ticksPerQuarterNote;

    int start;
    int left;
    int clipLen;
    const int delta = qRound(dx);
    const int quantize =
        snapStep(m_tempQuantizeOff, m_mouseDownStart + m_mouseDownClipStart + delta);
    const auto &timeline = appModel->timeline();
    if (m_mouseMoveBehavior == Move) {
        m_movedBeforeMouseUp = true;
        if (m_audioDragState) {
            const double cursorTick = (curPos.x() - m_scene->leftMarginPx()) / scaleX() /
                                      TracksEditorGlobal::pixelsPerQuarterNote *
                                      AppGlobal::ticksPerQuarterNote;
            const int desiredLeft =
                m_audioDragState->visibleStartForCursor(cursorTick, timeline);
            left = TimelineSnapUtils::snapNearest(
                desiredLeft, snapStep(m_tempQuantizeOff, desiredLeft), timeline);
            Clip::ClipCommonProperties properties(*m_currentEditingClip);
            m_audioDragState->moveTo(left, properties, timeline);
            m_currentEditingClip->setStart(properties.start);
            m_currentEditingClip->setClipStart(properties.clipStart);
            m_currentEditingClip->setClipLen(properties.clipLen);
            m_currentEditingClip->setLength(properties.length);
        } else {
            left = TimelineSnapUtils::snapNearest(m_mouseDownStart + m_mouseDownClipStart + delta,
                                                  quantize, timeline);
            start = left - m_mouseDownClipStart;
            m_currentEditingClip->setStart(start);
        }
        const auto targetTrackIndex = m_scene->trackIndexAt(curPos.y());
        if (targetTrackIndex >= 0) {
            m_currentEditingClip->setTrackIndex(targetTrackIndex);
            const auto colorIndex = appModel->tracks().at(targetTrackIndex)->colorIndex();
            m_currentEditingClip->setColorIndex(colorIndex);
            editorViewController->previewActiveClipTrackColor(colorIndex);
        }
    } else if (m_mouseMoveBehavior == ResizeLeft) {
        m_movedBeforeMouseUp = true;
        left = TimelineSnapUtils::snapNearest(m_mouseDownStart + m_mouseDownClipStart + delta,
                                              quantize, timeline);
        if (m_audioDragState) {
            Clip::ClipCommonProperties properties(*m_currentEditingClip);
            if (!m_audioDragState->resizeLeftTo(
                    left, m_mouseDownStart + m_mouseDownClipStart + m_mouseDownClipLen,
                    properties, timeline))
                return;
            m_currentEditingClip->setStart(properties.start);
            m_currentEditingClip->setClipStart(properties.clipStart);
            m_currentEditingClip->setClipLen(properties.clipLen);
            m_currentEditingClip->setLength(properties.length);
            return;
        }
        start = m_mouseDownStart;
        const int clipStart = left - start;
        clipLen = m_mouseDownStart + m_mouseDownClipStart + m_mouseDownClipLen - left;
        if (clipLen <= 0)
            return;

        if (clipStart < 0) {
            m_currentEditingClip->setClipStart(0);
            m_currentEditingClip->setClipLen(m_mouseDownClipStart + m_mouseDownClipLen);
        } else if (clipStart <= m_mouseDownClipStart + m_mouseDownClipLen) {
            m_currentEditingClip->setClipStart(clipStart);
            m_currentEditingClip->setClipLen(clipLen);
        } else {
            m_currentEditingClip->setClipStart(m_mouseDownClipStart + m_mouseDownClipLen);
            m_currentEditingClip->setClipLen(0);
        }
    } else if (m_mouseMoveBehavior == ResizeRight) {
        m_movedBeforeMouseUp = true;
        const int right = TimelineSnapUtils::snapNearest(m_mouseDownStart + m_mouseDownClipStart +
                                                             m_mouseDownClipLen + delta,
                                                         quantize, timeline);
        if (m_audioDragState) {
            const int visibleStart = m_mouseDownStart + m_mouseDownClipStart;
            Clip::ClipCommonProperties properties(*m_currentEditingClip);
            if (!m_audioDragState->resizeRightTo(right, visibleStart, properties, timeline))
                return;
            m_currentEditingClip->setStart(properties.start);
            m_currentEditingClip->setClipStart(properties.clipStart);
            m_currentEditingClip->setClipLen(properties.clipLen);
            m_currentEditingClip->setLength(properties.length);
            return;
        }
        clipLen = right - (m_mouseDownStart + m_mouseDownClipStart);
        if (clipLen <= 0)
            return;

        Clip::ClipCommonProperties properties(*m_currentEditingClip);
        if (ClipResizeUtils::updateRightEdge(properties, clipLen,
                                             m_currentEditingClip->canResizeLength(),
                                             m_currentEditingClip->contentLength())) {
            m_currentEditingClip->setLength(properties.length);
            m_currentEditingClip->setClipLen(properties.clipLen);
        }
    }
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

    if (m_mouseMoveBehavior != None && m_currentEditingClip) {
        // Extend the scene temporarily when dragging against the right edge so
        // the clip can travel past the current project tail.
        if (m_mouseMoveBehavior == Move || m_mouseMoveBehavior == ResizeRight) {
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
    if (m_mouseMoveBehavior != None && m_movedBeforeMouseUp && !cancelRequested)
        commitAction();
    else {
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
    if (m_currentEditingClip && m_movedBeforeMouseUp) {
        m_currentEditingClip->setStart(m_mouseDownStart);
        m_currentEditingClip->setClipStart(m_mouseDownClipStart);
        m_currentEditingClip->setLength(m_mouseDownLength);
        m_currentEditingClip->setClipLen(m_mouseDownClipLen);
        m_currentEditingClip->setTrackIndex(m_mouseDownTrackIndex);
        m_currentEditingClip->setColorIndex(m_mouseDownColorIndex);
        editorViewController->previewActiveClipTrackColor(m_mouseDownColorIndex);
    }
    editSessionManager->endActiveTransaction(EditSessionEndReason::Discard);
    resetEditState();
}

void TracksGraphicsView::commitAction() {
    if (m_currentEditingClip && m_movedBeforeMouseUp) {
        Clip::ClipCommonProperties args(*m_currentEditingClip);
        if (m_audioDragState)
            m_audioDragState->writeTruth(args);
        const int newTrackIndex = m_currentEditingClip->trackIndex();
        trackController->onClipPropertyChanged(args, newTrackIndex);
    }
    editSessionManager->endActiveTransaction(EditSessionEndReason::Commit);
    resetEditState();
}

void TracksGraphicsView::resetEditState() {
    m_mouseMoveBehavior = None;
    m_movedBeforeMouseUp = false;
    m_audioDragState.reset();
    m_currentEditingClip = nullptr;
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
            view->setSingerName(track->singerInfo().name());
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
    appStatus->selectedClips = ids;
    if (!ids.isEmpty()) {
        Track *track;
        appModel->findClipById(ids.first(), track);
        const auto trackIndex = appModel->tracks().indexOf(track);
        if (trackIndex >= 0)
            appStatus->selectedTrackIndex = trackIndex;
    }
}

void TracksGraphicsView::prepareForMovingOrResizingClip(const QMouseEvent *event,
                                                        AbstractClipView *clipItem) {
    const auto scenePos = mapToScene(event->pos());

    const bool ctrlDown = event->modifiers() == Qt::ControlModifier;
    if (!ctrlDown) {
        if (selectedClipItems().count() <= 1 || !selectedClipItems().contains(clipItem))
            clearSelections();
        clipItem->setSelected(true);
    } else {
        clipItem->setSelected(!clipItem->isSelected());
    }
    const auto rPos = clipItem->mapFromScene(scenePos);
    const auto rx = rPos.x();
    const auto edge = EditorResizeUtils::horizontalEdgeAt(
        rx, clipItem->rect().width(), AppGlobal::resizeTolerance);
    if (edge == EditorResizeUtils::HorizontalEdge::Left) {
        m_mouseMoveBehavior = ResizeLeft;
        clearSelections();
        clipItem->setSelected(true);
    } else if (edge == EditorResizeUtils::HorizontalEdge::Right) {
        m_mouseMoveBehavior = ResizeRight;
        clearSelections();
        clipItem->setSelected(true);
    } else {
        m_mouseMoveBehavior = Move;
    }

    m_currentEditingClip = clipItem;
    m_mouseDownPos = scenePos;
    m_mouseDownStart = m_currentEditingClip->start();
    m_mouseDownClipStart = m_currentEditingClip->clipStart();
    m_mouseDownLength = m_currentEditingClip->length();
    m_mouseDownClipLen = m_currentEditingClip->clipLen();
    m_mouseDownTrackIndex = m_currentEditingClip->trackIndex();
    m_mouseDownColorIndex = m_currentEditingClip->colorIndex();
    m_movedBeforeMouseUp = false;

    m_audioDragState.reset();
    if (clipItem->clipType() == IClip::Audio) {
        const auto audioClip = dynamic_cast<AudioClip *>(appModel->findClipById(clipItem->id()));
        if (audioClip && audioClip->hasRealTimeAnchor()) {
            const auto &timeline = appModel->timeline();
            const double grabTick = (scenePos.x() - m_scene->leftMarginPx()) / scaleX() /
                                    TracksEditorGlobal::pixelsPerQuarterNote *
                                    AppGlobal::ticksPerQuarterNote;
            m_audioDragState = AudioClipDragState::begin(
                audioClip->trimStartMs(), audioClip->playLengthMs(),
                audioClip->materialLengthMs(), m_mouseDownStart + m_mouseDownClipStart, grabTick,
                timeline);
        }
    }

    QList<int> clipIds;
    for (const auto clip : selectedClipItems())
        clipIds.append(clip->id());
    if (clipIds.isEmpty())
        clipIds.append(clipItem->id());
    editSessionManager->beginTransaction(AppStatus::EditObjectType::Clip, clipItem->id(), clipIds);
    appStatus->currentEditObject = AppStatus::EditObjectType::Clip;

    // Moving allows both axes (track change); resizing is horizontal only
    armEdgeAutoScroll(m_mouseMoveBehavior == Move ? (Qt::Horizontal | Qt::Vertical)
                                                  : Qt::Orientations(Qt::Horizontal),
                      event->pos());
}

AbstractClipView *TracksGraphicsView::findClipById(const int id) const {
    for (const auto item : m_scene->items())
        if (const auto clip = dynamic_cast<AbstractClipView *>(item))
            if (clip->id() == id)
                return clip;
    return nullptr;
}

void TracksGraphicsView::clearSelections() const {
    for (const auto item : m_scene->items())
        if (item->isSelected())
            item->setSelected(false);
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
