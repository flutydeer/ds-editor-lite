#include "RhiTrackCanvas.h"

#include "Controller/EditorViewController.h"
#include "Controller/PlaybackController.h"
#include "Controller/TrackController.h"
#include "Global/ControllerGlobal.h"
#include "Global/AppGlobal.h"
#include "Global/TracksEditorGlobal.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/ClipboardDataModel/ClipsInfo.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/EditSessionManager.h"
#include "Modules/Audio/AudioContext.h"
#include "Modules/Extractors/MidiExtractController.h"
#include "UI/Utils/AppColorPalette.h"
#include "UI/Utils/SpeakerMixDisplayUtils.h"
#include "UI/Views/EditorCanvas/RhiEditorCanvasWidget.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/Clip.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/MusicBase/TimelineSnapUtils.h>

#include <QAction>
#include <QClipboard>
#include <QDataStream>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QIODevice>
#include <QJsonDocument>
#include <QLocale>
#include <QMenu>
#include <QMimeData>
#include <QScrollBar>

#include <TalcsWidgets/AudioFileDialog.h>
#include <TalcsFormat/AbstractAudioFormatIO.h>
#include <TalcsFormat/FormatManager.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <limits>

RhiTrackCanvas::RhiTrackCanvas(QObject *parent)
    : ITrackEditorCanvas(parent),
      m_widget(new RhiEditorCanvasWidget(EditorCanvasKind::TrackEditor)) {
    connect(m_widget, &RhiEditorCanvasWidget::scaleChanged, this,
            &ITrackEditorCanvas::scaleChanged);
    connect(m_widget, &RhiEditorCanvasWidget::timeRangeChanged, this,
            &ITrackEditorCanvas::timeRangeChanged);
    connect(m_widget, &RhiEditorCanvasWidget::visibleRectChanged, this,
            &ITrackEditorCanvas::visibleRectChanged);
    connect(m_widget, &RhiEditorCanvasWidget::canvasSizeChanged, this,
            &ITrackEditorCanvas::sizeChanged);
    connect(m_widget, &RhiEditorCanvasWidget::backendFailed, this,
            &ITrackEditorCanvas::rendererFailed);
    connect(&m_scheduler, &RenderUpdateScheduler::updateRequested, this,
            &RhiTrackCanvas::publishSnapshot);
    connect(appStatus, &AppStatus::selectedTrackIndexChanged, this,
            [this] { refreshSnapshot(EditorDirtyDomain::Selection); });
    connect(appStatus, &AppStatus::clipSelectionChanged, this,
            [this] { refreshSnapshot(EditorDirtyDomain::Selection); });
    connect(appOptions, &AppOptions::optionsChanged, this,
            [this](const AppOptionsGlobal::Option option) {
                if (option == AppOptionsGlobal::DeveloperOptions)
                    refreshSnapshot(EditorDirtyDomain::Text);
            });
    connect(m_widget, &RhiEditorCanvasWidget::pointerPressed, this,
            &RhiTrackCanvas::onPointerPressed);
    connect(m_widget, &RhiEditorCanvasWidget::pointerMoved, this, &RhiTrackCanvas::onPointerMoved);
    connect(m_widget, &RhiEditorCanvasWidget::pointerLeft, this, [this] {
        if (m_hoveredClipId < 0)
            return;
        m_hoveredClipId = -1;
        refreshSnapshot(EditorDirtyDomain::Selection);
    });
    connect(m_widget, &RhiEditorCanvasWidget::pointerReleased, this,
            &RhiTrackCanvas::onPointerReleased);
    connect(m_widget, &RhiEditorCanvasWidget::pointerDoubleClicked, this,
            [this](const QPointF &position, const EditorHitResult &hit,
                   const Qt::MouseButton button,
                   const Qt::KeyboardModifiers) { onPointerDoubleClicked(position, hit, button); });
    connect(m_widget, &RhiEditorCanvasWidget::contextMenuRequested, this,
            &RhiTrackCanvas::showContextMenu);
    connect(m_widget, &RhiEditorCanvasWidget::keyPressed, this, &RhiTrackCanvas::onKeyPressed);
    connect(m_widget, &RhiEditorCanvasWidget::interactionCanceled, this,
            &RhiTrackCanvas::cancelInteraction);
    connect(m_widget, &RhiEditorCanvasWidget::visibleRectChanged, this,
            &RhiTrackCanvas::ensureVisibleSnapshot);
    connect(m_widget, &RhiEditorCanvasWidget::canvasSizeChanged, this,
            [this] { refreshSnapshot(EditorDirtyDomain::Geometry); });
    connect(appModel, &AppModel::modelChanged, this, [this] {
        clearAudioReaders();
        reconnectModelSignals();
        refreshSnapshot(EditorDirtyDomain::All);
    });
    connect(appModel, &AppModel::trackChanged, this,
            [this](const AppModel::TrackChangeType, const qsizetype, Track *) {
                reconnectModelSignals();
                refreshSnapshot(EditorDirtyDomain::Geometry);
            });
    connect(appModel, &AppModel::trackMoved, this, [this](const qsizetype, const qsizetype) {
        refreshSnapshot(EditorDirtyDomain::Geometry);
    });
    connect(appModel, &AppModel::timelineChanged, this,
            [this] { refreshSnapshot(EditorDirtyDomain::Waveform); });
    reconnectModelSignals();
    refreshSnapshot(EditorDirtyDomain::All);
}

RhiTrackCanvas::~RhiTrackCanvas() {
    finishEditTransaction(false);
    clearAudioReaders();
}

void RhiTrackCanvas::clearAudioReaders() {
    for (auto *reader : std::as_const(m_audioReaders)) {
        reader->close();
        delete reader;
    }
    m_audioReaders.clear();
}

void RhiTrackCanvas::reconnectModelSignals() {
    for (const auto &connection : std::as_const(m_modelConnections))
        disconnect(connection);
    m_modelConnections.clear();

    for (auto *track : appModel->tracks()) {
        m_modelConnections.append(connect(track, &Track::propertyChanged, this, [this] {
            refreshSnapshot(EditorDirtyDomain::Style | EditorDirtyDomain::Geometry);
        }));
        m_modelConnections.append(
            connect(track, &Track::clipChanged, this, [this](const Track::ClipChangeType, Clip *) {
                clearAudioReaders();
                reconnectModelSignals();
                refreshSnapshot(EditorDirtyDomain::Geometry | EditorDirtyDomain::Waveform);
            }));
        for (auto *clip : track->clips()) {
            m_modelConnections.append(connect(clip, &Clip::propertyChanged, this, [this] {
                refreshSnapshot(EditorDirtyDomain::Geometry | EditorDirtyDomain::Waveform);
            }));
            if (clip->clipType() == Clip::Singing) {
                auto *singingClip = static_cast<SingingClip *>(clip);
                m_modelConnections.append(
                    connect(singingClip, &SingingClip::noteChanged, this,
                            [this] { refreshSnapshot(EditorDirtyDomain::Geometry); }));
                m_modelConnections.append(connect(singingClip, &SingingClip::voiceContextChanged,
                                                  this, [this](const VoiceContextChange &) {
                                                      refreshSnapshot(EditorDirtyDomain::Text |
                                                                      EditorDirtyDomain::Style);
                                                  }));
            } else if (clip->clipType() == Clip::Audio) {
                auto *audioClip = static_cast<AudioClip *>(clip);
                m_modelConnections.append(connect(audioClip, &AudioClip::pathChanged, this, [this] {
                    clearAudioReaders();
                    refreshSnapshot(EditorDirtyDomain::Waveform);
                }));
            }
        }
    }
}

EditorCanvasBackend RhiTrackCanvas::backend() const {
    return EditorCanvasBackend::ExperimentalRhi;
}

QWidget *RhiTrackCanvas::widget() const {
    return m_widget;
}

QScrollBar *RhiTrackCanvas::horizontalScrollBar() const {
    return m_widget->horizontalScrollBar();
}

QScrollBar *RhiTrackCanvas::verticalScrollBar() const {
    return m_widget->verticalScrollBar();
}

EditorViewportState RhiTrackCanvas::viewportState() const {
    return m_widget->viewportState();
}

void RhiTrackCanvas::restoreViewportState(const EditorViewportState &state) {
    m_widget->restoreViewportState(state);
}

bool RhiTrackCanvas::centerAt(const double tick, const double trackIndex) {
    if (!std::isfinite(tick) || !std::isfinite(trackIndex))
        return false;
    m_widget->setViewportCenter(tick, trackIndex);
    return true;
}

bool RhiTrackCanvas::setViewportScale(const double horizontalScale, const double verticalScale) {
    return m_widget->setViewportScale(horizontalScale, verticalScale);
}

QRectF RhiTrackCanvas::visibleRect() const {
    const auto state = m_widget->viewportState();
    const auto x =
        state.startTick * TracksEditorGlobal::pixelsPerQuarterNote / AppGlobal::ticksPerQuarterNote;
    const auto width = (state.endTick - state.startTick) *
                       TracksEditorGlobal::pixelsPerQuarterNote / AppGlobal::ticksPerQuarterNote;
    const auto y = (state.topValue + 0.5) * TracksEditorGlobal::trackHeight;
    const auto height = (state.bottomValue - state.topValue) * TracksEditorGlobal::trackHeight;
    return {x, y, width, height};
}

double RhiTrackCanvas::sceneXForTick(const double tick) const {
    return tick * TracksEditorGlobal::pixelsPerQuarterNote / AppGlobal::ticksPerQuarterNote;
}

void RhiTrackCanvas::setSceneLength(const int tick) {
    m_sceneLength = qMax(0, tick);
    m_widget->setSceneLength(m_sceneLength);
}

void RhiTrackCanvas::setPlaybackPosition(const double tick) {
    m_widget->setPlaybackPosition(tick);
}

void RhiTrackCanvas::setLastPlaybackPosition(const double tick) {
    m_widget->setLastPlaybackPosition(tick);
}

void RhiTrackCanvas::refreshSnapshot(const EditorDirtyDomains domains) {
    m_scheduler.request(domains);
}

HistoryFocusVisibility RhiTrackCanvas::focusVisibility(const HistoryFocus &focus) const {
    if (focus.kind != HistoryFocusKind::TrackClips || !focus.isValid())
        return HistoryFocusVisibility::Unavailable;
    const auto visible = visibleRect();
    const auto left =
        focus.tickStart * TracksEditorGlobal::pixelsPerQuarterNote / AppGlobal::ticksPerQuarterNote;
    const auto right =
        focus.tickEnd * TracksEditorGlobal::pixelsPerQuarterNote / AppGlobal::ticksPerQuarterNote;
    const auto trackHeight = TracksEditorGlobal::trackHeight;
    const auto top = focus.trackIndex * trackHeight;
    const QRectF focusBounds(left, top, qMax(1.0, right - left), trackHeight);
    return visible.intersects(focusBounds) ? HistoryFocusVisibility::Visible
                                           : HistoryFocusVisibility::ScrollRequired;
}

bool RhiTrackCanvas::revealFocus(const HistoryFocus &focus, const bool animated) {
    Q_UNUSED(animated);
    if (focus.kind != HistoryFocusKind::TrackClips || !focus.isValid())
        return false;
    appStatus->selectedClips = focus.objectIds;
    if (!focus.objectIds.isEmpty())
        appStatus->activeClipId = focus.objectIds.first();
    auto trackIndex = focus.trackIndex;
    if (trackIndex < 0)
        trackIndex = qRound((focus.valueStart + focus.valueEnd) * 0.5);
    return centerAt((focus.tickStart + focus.tickEnd) * 0.5, trackIndex);
}

void RhiTrackCanvas::onWheelHorScale(QWheelEvent *event) {
    m_widget->onWheelHorScale(event);
}

void RhiTrackCanvas::onWheelVerScale(QWheelEvent *event) {
    m_widget->onWheelVerScale(event);
}

void RhiTrackCanvas::onWheelHorScroll(QWheelEvent *event) {
    m_widget->onWheelHorScroll(event);
}

void RhiTrackCanvas::onWheelVerScroll(QWheelEvent *event) {
    m_widget->onWheelVerScroll(event);
}

void RhiTrackCanvas::onPointerPressed(const QPointF &position, const EditorHitResult &hit,
                                      const Qt::MouseButton button,
                                      const Qt::KeyboardModifiers modifiers) {
    if (button != Qt::LeftButton)
        return;
    m_widget->setFocus(Qt::MouseFocusReason);
    m_pressPosition = position;
    m_pressedClipId = hit.objectId;
    m_selectionRect = {};

    int trackIndex = -1;
    auto *clip = findClip(hit.objectId, &trackIndex);
    if (!clip) {
        if (!modifiers.testFlag(Qt::ControlModifier))
            appStatus->selectedClips = QList<int>{};
        m_interaction = Interaction::RectSelect;
        m_selectionRect = QRectF(position, position);
        refreshSnapshot(EditorDirtyDomain::Selection | EditorDirtyDomain::Overlay);
        return;
    }

    auto selected = appStatus->selectedClips.get();
    if (modifiers.testFlag(Qt::ControlModifier)) {
        if (selected.contains(clip->id()))
            selected.removeAll(clip->id());
        else
            selected.append(clip->id());
        appStatus->selectedClips = selected;
        if (!selected.contains(clip->id()))
            return;
    } else if (!selected.contains(clip->id())) {
        appStatus->selectedClips = QList<int>{clip->id()};
    }
    trackController->setActiveClip(clip->id());
    appStatus->selectedTrackIndex = trackIndex;

    m_originalStart = clip->start();
    m_originalLength = clip->length();
    m_originalClipStart = clip->clipStart();
    m_originalClipLen = clip->clipLen();
    m_originalTrack = trackIndex;
    m_previewStart = m_originalStart;
    m_previewLength = m_originalLength;
    m_previewClipStart = m_originalClipStart;
    m_previewClipLen = m_originalClipLen;
    m_previewTrack = m_originalTrack;
    m_dragUsesRealtimeTruth = false;
    if (clip->clipType() == Clip::Audio) {
        const auto *audioClip = static_cast<const AudioClip *>(clip);
        if (audioClip->hasRealTimeAnchor()) {
            const auto &timeline = appModel->timeline();
            m_dragUsesRealtimeTruth = true;
            m_dragTrimMs = audioClip->trimStartMs();
            m_dragPlayLengthMs = audioClip->playLengthMs();
            m_dragMaterialLengthMs = audioClip->materialLengthMs();
            const auto visibleStartMs = timeline.tickToMs(m_originalStart + m_originalClipStart);
            m_materialStartMs = visibleStartMs - m_dragTrimMs;
            m_visibleEndMs = visibleStartMs + m_dragPlayLengthMs;
            m_grabOffsetMs = timeline.tickToMs(tickAt(position.x())) - visibleStartMs;
        }
    }
    if (hit.part == EditorHitResult::Part::LeftEdge)
        m_interaction = Interaction::ResizeLeft;
    else if (hit.part == EditorHitResult::Part::RightEdge)
        m_interaction = Interaction::ResizeRight;
    else
        m_interaction = Interaction::Move;
    beginEditTransaction(clip->id());
}

void RhiTrackCanvas::onPointerMoved(const QPointF &position, const EditorHitResult &hit,
                                    const Qt::MouseButtons buttons,
                                    const Qt::KeyboardModifiers modifiers) {
    if (m_hoveredClipId != hit.objectId) {
        m_hoveredClipId = hit.objectId;
        refreshSnapshot(EditorDirtyDomain::Selection);
    }
    if (!buttons.testFlag(Qt::LeftButton) || m_interaction == Interaction::None)
        return;
    autoScrollFor(position);

    if (m_interaction == Interaction::RectSelect) {
        m_selectionRect = QRectF(m_pressPosition, position).normalized();
        refreshSnapshot(EditorDirtyDomain::Overlay);
        return;
    }

    auto *clip = findClip(m_pressedClipId);
    if (!clip)
        return;
    const auto rawDelta = tickAt(position.x()) - tickAt(m_pressPosition.x());
    const auto originalLeft = m_originalStart + m_originalClipStart;
    const auto originalRight = originalLeft + m_originalClipLen;
    if (m_interaction == Interaction::Move) {
        auto left = snappedTick(originalLeft + rawDelta, modifiers.testFlag(Qt::AltModifier));
        if (m_dragUsesRealtimeTruth) {
            const auto &timeline = appModel->timeline();
            const auto cursorTick = tickAt(position.x());
            const auto desiredLeft =
                qRound(timeline.msToTick(timeline.tickToMs(cursorTick) - m_grabOffsetMs));
            left = snappedTick(desiredLeft, modifiers.testFlag(Qt::AltModifier));
            const auto caches = AudioClip::deriveTickCaches(m_dragTrimMs, m_dragPlayLengthMs,
                                                            m_dragMaterialLengthMs, left, timeline);
            m_previewStart = caches.start;
            m_previewLength = caches.length;
            m_previewClipStart = caches.clipStart;
            m_previewClipLen = caches.clipLen;
        } else {
            m_previewStart = left - m_originalClipStart;
        }
        m_previewTrack = trackAt(position.y());
        if (m_previewTrack < 0)
            m_previewTrack = m_originalTrack;
    } else if (m_interaction == Interaction::ResizeLeft) {
        auto left = snappedTick(originalLeft + rawDelta, modifiers.testFlag(Qt::AltModifier));
        if (left >= originalRight)
            return;
        if (m_dragUsesRealtimeTruth) {
            const auto &timeline = appModel->timeline();
            auto trim = timeline.tickToMs(left) - m_materialStartMs;
            if (trim < 0.0) {
                trim = 0.0;
                left = qRound(timeline.msToTick(m_materialStartMs));
            }
            m_dragTrimMs = trim;
            m_dragPlayLengthMs = m_visibleEndMs - timeline.tickToMs(left);
            const auto caches = AudioClip::deriveTickCaches(m_dragTrimMs, m_dragPlayLengthMs,
                                                            m_dragMaterialLengthMs, left, timeline);
            m_previewStart = caches.start;
            m_previewLength = caches.length;
            m_previewClipStart = caches.clipStart;
            m_previewClipLen = caches.clipLen;
            refreshSnapshot(EditorDirtyDomain::Overlay);
            return;
        }
        m_previewClipStart = qMax(0, left - m_originalStart);
        m_previewClipLen = originalRight - (m_originalStart + m_previewClipStart);
    } else if (m_interaction == Interaction::ResizeRight) {
        const auto right =
            snappedTick(originalRight + rawDelta, modifiers.testFlag(Qt::AltModifier));
        if (right <= originalLeft)
            return;
        if (m_dragUsesRealtimeTruth) {
            const auto &timeline = appModel->timeline();
            const auto visibleStartMs = m_materialStartMs + m_dragTrimMs;
            auto playLength = timeline.tickToMs(right) - visibleStartMs;
            playLength = qBound(0.0, playLength, m_dragMaterialLengthMs - m_dragTrimMs);
            if (playLength <= 0.0)
                return;
            m_dragPlayLengthMs = playLength;
            const auto caches = AudioClip::deriveTickCaches(
                m_dragTrimMs, m_dragPlayLengthMs, m_dragMaterialLengthMs, originalLeft, timeline);
            m_previewStart = caches.start;
            m_previewLength = caches.length;
            m_previewClipStart = caches.clipStart;
            m_previewClipLen = caches.clipLen;
            refreshSnapshot(EditorDirtyDomain::Overlay);
            return;
        }
        m_previewClipLen = right - originalLeft;
        if (clip->clipType() == Clip::Audio) {
            m_previewClipLen = qMin(m_previewClipLen, m_originalLength - m_originalClipStart);
        } else {
            m_previewLength = qMax(m_originalLength, m_originalClipStart + m_previewClipLen);
        }
    }
    refreshSnapshot(EditorDirtyDomain::Overlay);
}

void RhiTrackCanvas::onPointerReleased(const QPointF &position, const EditorHitResult &hit,
                                       const Qt::MouseButton button,
                                       const Qt::KeyboardModifiers modifiers) {
    Q_UNUSED(position);
    Q_UNUSED(hit);
    if (button != Qt::LeftButton || m_interaction == Interaction::None)
        return;

    auto changed = false;
    if (m_interaction == Interaction::RectSelect && !m_selectionRect.isNull()) {
        auto selected =
            modifiers.testFlag(Qt::ControlModifier) ? appStatus->selectedClips.get() : QList<int>{};
        const auto tracks = appModel->tracks();
        for (qsizetype trackIndex = 0; trackIndex < tracks.size(); ++trackIndex) {
            for (const auto *clip : tracks.at(trackIndex)->clips()) {
                const auto left = sceneXForTick(clip->start() + clip->clipStart());
                const auto width = sceneXForTick(clip->clipLen()) - sceneXForTick(0);
                const QRectF bounds(left, trackIndex * TracksEditorGlobal::trackHeight, width,
                                    TracksEditorGlobal::trackHeight);
                if (m_selectionRect.intersects(bounds) && !selected.contains(clip->id()))
                    selected.append(clip->id());
            }
        }
        appStatus->selectedClips = selected;
    } else if (auto *clip = findClip(m_pressedClipId)) {
        Clip::ClipCommonProperties args(*clip);
        args.start = m_previewStart;
        args.length = m_previewLength;
        args.clipStart = m_previewClipStart;
        args.clipLen = m_previewClipLen;
        if (m_dragUsesRealtimeTruth) {
            args.trimStartMs = m_dragTrimMs;
            args.playLengthMs = m_dragPlayLengthMs;
            args.materialLengthMs = m_dragMaterialLengthMs;
        }
        changed = args.start != m_originalStart || args.length != m_originalLength ||
                  args.clipStart != m_originalClipStart || args.clipLen != m_originalClipLen ||
                  m_previewTrack != m_originalTrack;
        if (changed)
            trackController->onClipPropertyChanged(args, m_previewTrack);
    }
    finishEditTransaction(changed);
    m_interaction = Interaction::None;
    m_dragUsesRealtimeTruth = false;
    m_selectionRect = {};
    m_widget->setSceneLength(m_sceneLength);
    refreshSnapshot(EditorDirtyDomain::All);
}

void RhiTrackCanvas::onPointerDoubleClicked(const QPointF &position, const EditorHitResult &hit,
                                            const Qt::MouseButton button) {
    if (button != Qt::LeftButton)
        return;
    if (hit.isValid()) {
        trackController->setActiveClip(hit.objectId);
        editorViewController->showBottomPanelPage(QStringLiteral("ClipEditor"));
        editorViewController->centerPianoRollAt(playbackController->position(), 60);
        return;
    }
    const auto trackIndex = trackAt(position.y());
    if (trackIndex < 0)
        return;
    trackController->onNewSingingClip(trackIndex, snappedTick(tickAt(position.x()), false));
}

void RhiTrackCanvas::showContextMenu(const QPointF &position, const EditorHitResult &hit,
                                     const QPoint &globalPosition) {
    const auto trackIndex = trackAt(position.y());
    if (trackIndex < 0)
        return;
    QMenu menu(m_widget);
    if (hit.isValid()) {
        if (!appStatus->selectedClips.get().contains(hit.objectId))
            appStatus->selectedClips = QList<int>{hit.objectId};
        if (const auto *clip = findClip(hit.objectId); clip && clip->clipType() == Clip::Audio) {
            const auto *audioClip = static_cast<const AudioClip *>(clip);
            const auto relinkAction = menu.addAction(tr("Relink Audio File..."));
            connect(relinkAction, &QAction::triggered, this,
                    [this, id = hit.objectId] { relocateAudioClip(id); });
            const auto extractAction = menu.addAction(tr("Extract MIDI Score"));
            connect(extractAction, &QAction::triggered, this,
                    [id = hit.objectId] { extractMidi(id); });
            if (audioClip->pathStatus() == AudioClip::PathStatus::Missing) {
                menu.removeAction(relinkAction);
                menu.insertAction(menu.actions().constFirst(), relinkAction);
            }
            menu.addSeparator();
        }
        const auto cutAction = menu.addAction(tr("Cu&t"));
        connect(cutAction, &QAction::triggered, trackController,
                &TrackController::cutSelectedClips);
        const auto copyAction = menu.addAction(tr("&Copy"));
        connect(copyAction, &QAction::triggered, trackController,
                &TrackController::copySelectedClips);
        const auto deleteAction = menu.addAction(tr("&Delete"));
        connect(deleteAction, &QAction::triggered, this,
                [] { trackController->onRemoveClips(appStatus->selectedClips); });
    } else {
        const auto tick = snappedTick(tickAt(position.x()), false);
        const auto newClipAction = menu.addAction(tr("New singing clip"));
        connect(newClipAction, &QAction::triggered, this,
                [trackIndex, tick] { trackController->onNewSingingClip(trackIndex, tick); });
        const auto audioAction = menu.addAction(tr("Insert audio clip..."));
        connect(audioAction, &QAction::triggered, this,
                [this, trackIndex, tick] { addAudioClip(trackIndex, tick); });
        menu.addSeparator();
        const auto pasteAction = menu.addAction(tr("&Paste"));
        const auto *mimeData = QGuiApplication::clipboard()->mimeData();
        const auto hasClipData =
            mimeData &&
            mimeData->hasFormat(ControllerGlobal::ElemMimeType.at(ControllerGlobal::Clip));
        pasteAction->setEnabled(hasClipData);
        if (hasClipData) {
            const auto json = QJsonDocument::fromJson(
                mimeData->data(ControllerGlobal::ElemMimeType.at(ControllerGlobal::Clip)));
            const auto info = ClipsInfo::deserializeFromJson(json.object());
            const auto previewTick = snappedTick(tick, false);
            auto firstClipStart = INT_MAX;
            for (const auto *clip : info.clips)
                firstClipStart = qMin(firstClipStart, clip->start());
            connect(pasteAction, &QAction::triggered, this, [info, tick, trackIndex] {
                trackController->pasteClips(info, tick, trackIndex);
            });
            connect(pasteAction, &QAction::hovered, this,
                    [this, info, previewTick, trackIndex, firstClipStart] {
                        if (!m_pastePreviewRects.isEmpty())
                            return;
                        for (qsizetype i = 0; i < info.clips.size(); ++i) {
                            const auto *clip = info.clips.at(i);
                            const auto targetTrack =
                                qBound(0, trackIndex + info.trackIndexOffsets.value(i),
                                       static_cast<int>(appModel->tracks().size()) - 1);
                            const auto targetStart =
                                previewTick + clip->start() - firstClipStart + clip->clipStart();
                            auto color = AppColorPalette::instance()->clipBackground(
                                appModel->tracks().at(targetTrack)->colorIndex());
                            color.setAlpha(90);
                            m_pastePreviewRects.append({
                                .objectId = -1,
                                .bounds =
                                    {
                                             sceneXForTick(targetStart),
                                             targetTrack * TracksEditorGlobal::trackHeight + 3.0,
                                             qMax(1.0, sceneXForTick(clip->clipLen())),
                                             TracksEditorGlobal::trackHeight - 6.0,
                                             },
                                .fill = color,
                                .border = QColor(220, 230, 250, 140),
                                .layer = 30,
                            });
                        }
                        refreshSnapshot(EditorDirtyDomain::Overlay);
                    });
            connect(&menu, &QMenu::aboutToHide, this, &RhiTrackCanvas::clearPastePreview);
        }
    }
    menu.exec(globalPosition);
}

void RhiTrackCanvas::onKeyPressed(const int key, const Qt::KeyboardModifiers modifiers) {
    if (key == Qt::Key_Delete || key == Qt::Key_Backspace) {
        trackController->onRemoveClips(appStatus->selectedClips);
    } else if (modifiers.testFlag(Qt::ControlModifier) && key == Qt::Key_C) {
        trackController->copySelectedClips();
    } else if (modifiers.testFlag(Qt::ControlModifier) && key == Qt::Key_X) {
        trackController->cutSelectedClips();
    } else if (modifiers.testFlag(Qt::ControlModifier) && key == Qt::Key_V) {
        const auto *mimeData = QGuiApplication::clipboard()->mimeData();
        if (!mimeData ||
            !mimeData->hasFormat(ControllerGlobal::ElemMimeType.at(ControllerGlobal::Clip)))
            return;
        const auto info = ClipsInfo::deserializeFromJson(
            QJsonDocument::fromJson(
                mimeData->data(ControllerGlobal::ElemMimeType.at(ControllerGlobal::Clip)))
                .object());
        const auto trackIndex = qBound(0, static_cast<int>(appStatus->selectedTrackIndex),
                                       static_cast<int>(appModel->tracks().size()) - 1);
        trackController->pasteClips(info, qRound(m_widget->startTick()), trackIndex);
    }
}

void RhiTrackCanvas::cancelInteraction() {
    if (m_interaction == Interaction::None)
        return;
    finishEditTransaction(false);
    m_interaction = Interaction::None;
    m_dragUsesRealtimeTruth = false;
    m_selectionRect = {};
    m_widget->setSceneLength(m_sceneLength);
    refreshSnapshot(EditorDirtyDomain::Overlay);
}

void RhiTrackCanvas::autoScrollFor(const QPointF &logicalPosition) {
    const auto camera = m_widget->viewportState();
    const auto viewportX =
        (logicalPosition.x() - sceneXForTick(camera.startTick)) * m_widget->scaleX();
    const auto viewportY = (logicalPosition.y() - visibleRect().top()) * m_widget->scaleY();
    constexpr int margin = 28;
    constexpr int step = 18;
    if ((m_interaction == Interaction::Move || m_interaction == Interaction::ResizeRight) &&
        viewportX > m_widget->width() - margin &&
        m_widget->horizontalScrollBar()->value() >= m_widget->horizontalScrollBar()->maximum()) {
        const auto span = qMax(1.0, camera.endTick - camera.startTick);
        m_widget->setSceneLength(qMax(m_sceneLength, qRound(camera.endTick + span)));
    }
    if (viewportX < margin)
        m_widget->horizontalScrollBar()->setValue(m_widget->horizontalScrollBar()->value() - step);
    else if (viewportX > m_widget->width() - margin)
        m_widget->horizontalScrollBar()->setValue(m_widget->horizontalScrollBar()->value() + step);
    if (m_interaction == Interaction::Move) {
        if (viewportY < margin)
            m_widget->verticalScrollBar()->setValue(m_widget->verticalScrollBar()->value() - step);
        else if (viewportY > m_widget->height() - margin)
            m_widget->verticalScrollBar()->setValue(m_widget->verticalScrollBar()->value() + step);
    }
}

void RhiTrackCanvas::clearPastePreview() {
    if (m_pastePreviewRects.isEmpty())
        return;
    m_pastePreviewRects.clear();
    refreshSnapshot(EditorDirtyDomain::Overlay);
}

void RhiTrackCanvas::addAudioClip(const int trackIndex, const int tick) {
    QString fileName;
    QVariant userData;
    QString entryClassName;
    auto *io = talcs::AudioFileDialog::getOpenAudioFileIO(
        AudioContext::instance()->formatManager(), fileName, userData, entryClassName, m_widget,
        tr("Select an Audio File"), QStringLiteral("."));
    if (fileName.isNull()) {
        delete io;
        return;
    }
    QByteArray dataBuffer;
    QDataStream stream(&dataBuffer, QIODevice::WriteOnly);
    stream << userData;
    const QJsonObject workspace{
        {"userData",       QString::fromLatin1(dataBuffer.toBase64())},
        {"entryClassName", entryClassName                            },
    };
    trackController->onAddAudioClip(fileName, io, workspace,
                                    appModel->tracks().at(trackIndex)->id(), tick);
}

void RhiTrackCanvas::relocateAudioClip(const int clipId) {
    QString fileName;
    QVariant userData;
    QString entryClassName;
    auto *io = talcs::AudioFileDialog::getOpenAudioFileIO(
        AudioContext::instance()->formatManager(), fileName, userData, entryClassName, m_widget,
        tr("Select an Audio File"), QStringLiteral("."));
    if (fileName.isNull()) {
        delete io;
        return;
    }
    QByteArray dataBuffer;
    QDataStream stream(&dataBuffer, QIODevice::WriteOnly);
    stream << userData;
    const QJsonObject workspace{
        {"userData",       QString::fromLatin1(dataBuffer.toBase64())},
        {"entryClassName", entryClassName                            },
    };
    trackController->onRelocateAudioClip(clipId, fileName, io, workspace);
}

void RhiTrackCanvas::extractMidi(const int clipId) {
    auto *audioClip = dynamic_cast<AudioClip *>(appModel->findClipById(clipId));
    if (audioClip)
        midiExtractController->runExtractMidi(audioClip);
}

talcs::AbstractAudioFormatIO *RhiTrackCanvas::audioReader(const QString &path) {
    if (path.isEmpty())
        return nullptr;
    if (const auto it = m_audioReaders.constFind(path); it != m_audioReaders.cend())
        return it.value();
    auto *reader = AudioContext::instance()->formatManager()->getFormatLoad(path);
    if (!reader || !reader->open(talcs::AbstractAudioFormatIO::Read)) {
        delete reader;
        return nullptr;
    }
    m_audioReaders.insert(path, reader);
    return reader;
}

double RhiTrackCanvas::sincInterpolate(const QVector<float> &samples, const qint64 sampleOffset,
                                       const qint64 totalFrames, const double position) {
    constexpr int halfKernel = 16;
    constexpr double pi = 3.14159265358979323846;
    const auto center = static_cast<qint64>(std::floor(position));
    const auto fraction = position - center;
    auto result = 0.0;
    for (auto offset = -halfKernel; offset <= halfKernel; ++offset) {
        const auto sampleIndex = center + offset;
        const auto bufferIndex = sampleIndex - sampleOffset;
        if (sampleIndex < 0 || sampleIndex >= totalFrames || bufferIndex < 0 ||
            bufferIndex >= samples.size())
            continue;
        const auto x = fraction - offset;
        auto sinc = 1.0;
        auto window = 1.0;
        if (std::abs(x) >= 1e-9) {
            sinc = std::sin(pi * x) / (pi * x);
            const auto windowX = x / halfKernel;
            window = std::abs(windowX) < 1.0 ? std::sin(pi * windowX) / (pi * windowX) : 0.0;
        }
        result += samples.at(static_cast<qsizetype>(bufferIndex)) * sinc * window;
    }
    return result;
}

void RhiTrackCanvas::appendAudioWaveform(EditorRenderSnapshot &snapshot, const AudioClip *clip,
                                         const QRectF &bounds, const int materialStartTick,
                                         const QColor &color) {
    const auto &info = clip->audioInfo();
    if (info.sampleRate <= 0 || info.channels <= 0 || info.frames <= 0 || bounds.width() <= 0.0 ||
        bounds.height() <= 0.0)
        return;

    const auto visibleLeft = qMax(bounds.left(), sceneXForTick(m_cachedStartTick));
    const auto visibleRight = qMin(bounds.right(), sceneXForTick(m_cachedEndTick));
    if (visibleLeft >= visibleRight)
        return;

    const auto &timeline = appModel->timeline();
    const auto sampleAtSceneX = [&](const double sceneX) {
        const auto tick =
            sceneX * AppGlobal::ticksPerQuarterNote / TracksEditorGlobal::pixelsPerQuarterNote;
        return (timeline.tickToMs(tick) - timeline.tickToMs(materialStartTick)) * info.sampleRate /
               1000.0;
    };
    const auto sceneXAtSample = [&](const double sample) {
        const auto tick = timeline.msToTick(timeline.tickToMs(materialStartTick) +
                                            sample * 1000.0 / info.sampleRate);
        return sceneXForTick(tick);
    };
    const auto physicalScale = m_widget->scaleX() * m_widget->devicePixelRatioF();
    const auto physicalPointCount =
        qBound(1, qCeil((visibleRight - visibleLeft) * physicalScale), 2048);
    const auto samplesPerPhysicalPixel =
        std::abs(sampleAtSceneX(visibleLeft + 1.0 / physicalScale) - sampleAtSceneX(visibleLeft));
    const auto centerY = bounds.center().y();
    const auto halfHeight = bounds.height() * 0.5;

    if (info.chunkSize > 0 && samplesPerPhysicalPixel > info.chunkSize) {
        const auto useMipmap = !info.peakCacheMipmap.isEmpty() && info.mipmapScale > 0 &&
                               samplesPerPhysicalPixel >= info.chunkSize * info.mipmapScale;
        const auto &peaks = useMipmap ? info.peakCacheMipmap : info.peakCache;
        const auto framesPerPeak =
            static_cast<double>(info.chunkSize) * (useMipmap ? info.mipmapScale : 1);
        for (auto pointIndex = 0; pointIndex < physicalPointCount && !peaks.isEmpty();
             ++pointIndex) {
            const auto left =
                visibleLeft + pointIndex * (visibleRight - visibleLeft) / physicalPointCount;
            const auto right = visibleLeft + (pointIndex + 1.0) * (visibleRight - visibleLeft) /
                                                 physicalPointCount;
            const auto firstPeak = qMax(0, qFloor(sampleAtSceneX(left) / framesPerPeak));
            const auto lastPeak =
                qMin(static_cast<int>(peaks.size()),
                     qMax(firstPeak + 1, qCeil(sampleAtSceneX(right) / framesPerPeak)));
            short minimum = 0;
            short maximum = 0;
            for (auto peakIndex = firstPeak; peakIndex < lastPeak; ++peakIndex) {
                minimum = qMin(minimum, std::get<0>(peaks.at(peakIndex)));
                maximum = qMax(maximum, std::get<1>(peaks.at(peakIndex)));
            }
            const auto x = (left + right) * 0.5;
            snapshot.lines.append({
                {x, centerY - maximum / 32767.0 * halfHeight},
                {x, centerY - minimum / 32767.0 * halfHeight},
                color,
                1.0F,
                11,
            });
        }
        return;
    }

    auto *reader = audioReader(clip->path());
    if (!reader)
        return;
    constexpr int halfKernel = 16;
    auto firstSample = static_cast<qint64>(std::floor(sampleAtSceneX(visibleLeft)));
    auto lastSample = static_cast<qint64>(std::ceil(sampleAtSceneX(visibleRight)));
    const auto curveMode = samplesPerPhysicalPixel <= 4.0;
    if (curveMode) {
        firstSample -= halfKernel;
        lastSample += halfKernel;
    }
    firstSample = qBound<qint64>(0, firstSample, info.frames);
    lastSample = qBound<qint64>(0, lastSample, info.frames);
    if (lastSample <= firstSample)
        return;

    const auto requestedFrames = lastSample - firstSample;
    if (requestedFrames > std::numeric_limits<int>::max() / info.channels)
        return;
    QVector<float> interleaved(static_cast<int>(requestedFrames * info.channels));
    reader->seek(firstSample);
    const auto framesRead = reader->read(interleaved.data(), requestedFrames);
    if (framesRead <= 0)
        return;

    QVector<float> mono(static_cast<int>(framesRead));
    for (auto frame = 0; frame < framesRead; ++frame) {
        auto value = 0.0F;
        const auto sourceOffset = static_cast<int>(frame * info.channels);
        for (auto channel = 0; channel < info.channels; ++channel)
            value += interleaved.at(sourceOffset + channel);
        mono[static_cast<int>(frame)] = value / info.channels;
    }

    if (!curveMode) {
        for (auto pointIndex = 0; pointIndex < physicalPointCount; ++pointIndex) {
            const auto left =
                visibleLeft + pointIndex * (visibleRight - visibleLeft) / physicalPointCount;
            const auto right = visibleLeft + (pointIndex + 1.0) * (visibleRight - visibleLeft) /
                                                 physicalPointCount;
            const auto sampleBegin =
                qBound<qint64>(firstSample, static_cast<qint64>(std::floor(sampleAtSceneX(left))),
                               firstSample + framesRead);
            if (sampleBegin >= firstSample + framesRead)
                continue;
            const auto sampleEnd = qBound<qint64>(
                sampleBegin + 1, static_cast<qint64>(std::ceil(sampleAtSceneX(right))),
                firstSample + framesRead);
            auto minimum = 0.0F;
            auto maximum = 0.0F;
            for (auto sample = sampleBegin; sample < sampleEnd; ++sample) {
                const auto value = mono.at(static_cast<qsizetype>(sample - firstSample));
                minimum = qMin(minimum, value);
                maximum = qMax(maximum, value);
            }
            const auto x = (left + right) * 0.5;
            snapshot.lines.append({
                {x, centerY - maximum * halfHeight},
                {x, centerY - minimum * halfHeight},
                color,
                1.0F,
                11,
            });
        }
        return;
    }

    constexpr auto curveOversample = 3;
    EditorRenderPath path;
    path.color = color;
    path.width = 1.0F;
    path.join = EditorStrokeJoin::Round;
    path.cap = EditorStrokeCap::Round;
    path.layer = 11;
    const auto curvePointCount = physicalPointCount * curveOversample + 1;
    path.points.reserve(curvePointCount);
    for (auto pointIndex = 0; pointIndex < curvePointCount; ++pointIndex) {
        const auto x =
            visibleLeft + pointIndex * (visibleRight - visibleLeft) / (curvePointCount - 1);
        const auto value = sincInterpolate(mono, firstSample, info.frames, sampleAtSceneX(x));
        path.points.append(
            {x, qBound(bounds.top(), centerY - value * halfHeight, bounds.bottom())});
    }
    snapshot.paths.append(path);

    const auto samplesPerLogicalPixel = samplesPerPhysicalPixel * physicalScale;
    if (samplesPerLogicalPixel > 0.0 && samplesPerLogicalPixel < 1.0 / 6.0) {
        const auto radius = qMin(3.0, 0.3 / samplesPerLogicalPixel);
        const auto firstVisibleSample =
            qMax<qint64>(0, static_cast<qint64>(std::floor(sampleAtSceneX(visibleLeft))));
        const auto lastVisibleSample =
            qMin<qint64>(info.frames, static_cast<qint64>(std::ceil(sampleAtSceneX(visibleRight))));
        for (auto sample = firstVisibleSample; sample < lastVisibleSample; ++sample) {
            const auto bufferIndex = sample - firstSample;
            if (bufferIndex < 0 || bufferIndex >= mono.size())
                continue;
            const auto x = sceneXAtSample(sample);
            const auto y = qBound(
                bounds.top(), centerY - mono.at(static_cast<qsizetype>(bufferIndex)) * halfHeight,
                bounds.bottom());
            snapshot.rectangles.append({
                .objectId = -1,
                .bounds = {x - radius, y - radius, radius * 2.0, radius * 2.0},
                .fill = color,
                .layer = 11,
            });
        }
    }
}

Clip *RhiTrackCanvas::findClip(const int id, int *trackIndex) const {
    if (id < 0)
        return nullptr;
    int foundTrack = -1;
    auto *clip = appModel->findClipById(id, foundTrack);
    if (trackIndex)
        *trackIndex = foundTrack;
    return clip;
}

int RhiTrackCanvas::tickAt(const double x) const {
    return qRound(x * AppGlobal::ticksPerQuarterNote / TracksEditorGlobal::pixelsPerQuarterNote);
}

int RhiTrackCanvas::trackAt(const double y) const {
    const auto index = static_cast<int>(std::floor(y / TracksEditorGlobal::trackHeight));
    return index >= 0 && index < appModel->tracks().size() ? index : -1;
}

int RhiTrackCanvas::snappedTick(const int tick, const bool snapOff) const {
    const auto step = snapOff ? 1 : TimelineSnapUtils::quantizeToTicks(128);
    return TimelineSnapUtils::snapNearest(tick, step, appModel->timeline());
}

void RhiTrackCanvas::beginEditTransaction(const int clipId) {
    if (editSessionManager->hasActiveTransaction())
        return;
    const auto selected = appStatus->selectedClips.get();
    editSessionManager->beginTransaction(AppStatus::EditObjectType::Clip, clipId,
                                         selected.isEmpty() ? QList<int>{clipId} : selected);
    appStatus->currentEditObject = AppStatus::EditObjectType::Clip;
    m_editTransactionActive = true;
}

void RhiTrackCanvas::finishEditTransaction(const bool commit) {
    if (!m_editTransactionActive)
        return;
    editSessionManager->endActiveTransaction(commit ? EditSessionEndReason::Commit
                                                    : EditSessionEndReason::Discard);
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
    m_editTransactionActive = false;
}

ImmutableEditorRenderSnapshot RhiTrackCanvas::buildSnapshot() {
    auto snapshot = std::make_shared<EditorRenderSnapshot>();
    snapshot->kind = EditorCanvasKind::TrackEditor;
    snapshot->revision = m_revision + 1;
    snapshot->selectedIds = appStatus->selectedClips;
    snapshot->hoveredId = m_hoveredClipId;

    const auto tracks = appModel->tracks();
    const auto trackHeight = static_cast<double>(TracksEditorGlobal::trackHeight);
    auto maximumTick = static_cast<double>(appStatus->projectEditableLength);
    const auto selectedTrack = static_cast<int>(appStatus->selectedTrackIndex);
    const auto viewport = m_widget->viewportState();
    const auto viewportTickSpan = qMax(static_cast<double>(AppGlobal::ticksPerWholeNote * 2),
                                       viewport.endTick - viewport.startTick);
    m_cachedStartTick = qMax(0.0, viewport.startTick - viewportTickSpan);
    m_cachedEndTick = viewport.endTick + viewportTickSpan;
    const auto viewportTrackSpan = qMax(4.0, viewport.bottomValue - viewport.topValue);
    m_cachedTopTrack = qMax(-0.5, viewport.topValue - viewportTrackSpan);
    m_cachedBottomTrack =
        qMin(static_cast<double>(tracks.size()) - 0.5, viewport.bottomValue + viewportTrackSpan);

    for (const auto *track : tracks) {
        for (const auto *clip : track->clips()) {
            const auto endTick = clip->start() + clip->clipStart() + clip->clipLen();
            maximumTick = qMax(maximumTick, static_cast<double>(endTick));
        }
    }
    const auto width = qMax(maximumTick * TracksEditorGlobal::pixelsPerQuarterNote /
                                static_cast<double>(AppGlobal::ticksPerQuarterNote),
                            1.0);
    snapshot->logicalExtent = {
        width,
        qMax(trackHeight, static_cast<double>(tracks.size()) * trackHeight),
    };

    for (qsizetype trackIndex = 0; trackIndex < tracks.size(); ++trackIndex) {
        if (trackIndex + 0.5 < m_cachedTopTrack || trackIndex - 0.5 > m_cachedBottomTrack)
            continue;
        const auto *track = tracks.at(trackIndex);
        const auto rowTop = trackIndex * trackHeight;
        snapshot->rectangles.append({
            .objectId = -1,
            .bounds = {0.0, rowTop, width, trackHeight},
            .fill = static_cast<int>(trackIndex) == selectedTrack ? QColor(48, 52, 63)
                                                                  : QColor(28, 31, 38),
            .border = QColor(55, 60, 70),
        });
        for (const auto *clip : track->clips()) {
            auto start = clip->start();
            auto length = clip->length();
            auto clipStart = clip->clipStart();
            auto clipLen = clip->clipLen();
            auto previewTrack = static_cast<int>(trackIndex);
            if (clip->id() == m_pressedClipId && m_interaction != Interaction::None &&
                m_interaction != Interaction::RectSelect) {
                start = m_previewStart;
                length = m_previewLength;
                clipStart = m_previewClipStart;
                clipLen = m_previewClipLen;
                previewTrack = m_previewTrack;
            }
            const auto startTick = start + clipStart;
            const auto endTick = startTick + clipLen;
            if (endTick < m_cachedStartTick || startTick > m_cachedEndTick)
                continue;
            const auto x = startTick * TracksEditorGlobal::pixelsPerQuarterNote /
                           AppGlobal::ticksPerQuarterNote;
            const auto clipWidth =
                qMax(1.0, clipLen * TracksEditorGlobal::pixelsPerQuarterNote /
                              static_cast<double>(AppGlobal::ticksPerQuarterNote));
            const auto selected = snapshot->selectedIds.contains(clip->id());
            const auto palette = AppColorPalette::instance();
            const auto colorIndex = previewTrack >= 0 && previewTrack < tracks.size()
                                        ? tracks.at(previewTrack)->colorIndex()
                                        : track->colorIndex();
            EditorRenderRect clipRect;
            clipRect.objectId = clip->id();
            clipRect.bounds =
                QRectF(x, previewTrack * trackHeight + 3.0, clipWidth, trackHeight - 6.0);
            clipRect.fill = selected ? palette->clipBackgroundSelected(colorIndex)
                                     : palette->clipBackground(colorIndex);
            clipRect.border =
                selected || clip->id() == m_hoveredClipId || clip->id() == appStatus->activeClipId
                    ? QColor(235, 238, 245)
                    : palette->clipBorder(colorIndex);
            clipRect.selected = selected;
            clipRect.hovered = clip->id() == m_hoveredClipId;
            clipRect.layer = 10;
            snapshot->rectangles.append(clipRect);

            QFont clipFont;
            clipFont.setPixelSize(12);
            const auto showDebug = appOptions->developer()->showClipDebugInfo;
            auto clipText = QStringLiteral("%1 %2 %3dB %4")
                                .arg(clip->clipType() == Clip::Singing ? QStringLiteral("♪")
                                                                       : QStringLiteral("▣"),
                                     clip->name(), QLocale().toString(clip->gain()),
                                     clip->mute() ? QStringLiteral("M") : QString());
            if (showDebug) {
                clipText += QStringLiteral(" id:%1 s:%2 l:%3 cs:%4 cl:%5 sx:%6 sy:%7")
                                .arg(clip->id())
                                .arg(start)
                                .arg(length)
                                .arg(clipStart)
                                .arg(clipLen)
                                .arg(m_widget->scaleX())
                                .arg(m_widget->scaleY());
            }
            snapshot->texts.append({
                .text = clipText,
                .baseline = {clipRect.bounds.left() + 6.0, clipRect.bounds.top() + 17.0},
                .color = clip->mute() ? QColor(170, 175, 185) : QColor(245, 247, 251),
                .font = clipFont,
                .clip = clipRect.bounds.adjusted(4.0, 2.0, -4.0, -2.0),
                .layer = 12,
            });

            const auto previewBounds = clipRect.bounds.adjusted(3.0, 22.0, -3.0, -3.0);
            const auto previewColor = palette->clipForeground(colorIndex);
            if (previewBounds.height() > 4.0 && clip->clipType() == Clip::Singing) {
                const auto *singingClip = static_cast<const SingingClip *>(clip);
                QFont detailFont;
                detailFont.setPixelSize(10);
                const auto speaker = SpeakerMixDisplayUtils::speakerDisplayName(
                    singingClip->singerInfo(), singingClip->speakerInfo(),
                    singingClip->speakerMixData());
                snapshot->texts.append({
                    .text = speaker.isEmpty() ? singingClip->singerInfo().name()
                                              : QStringLiteral("%1 · %2").arg(
                                                    singingClip->singerInfo().name(), speaker),
                    .baseline = {clipRect.bounds.left() + 6.0, clipRect.bounds.top() + 31.0},
                    .color = QColor(225, 230, 240, 185),
                    .font = detailFont,
                    .clip = clipRect.bounds.adjusted(4.0, 18.0, -4.0, -2.0),
                    .layer = 12,
                });
                auto lowestKey = 127;
                auto highestKey = 0;
                for (const auto *note : singingClip->notes()) {
                    lowestKey = qMin(lowestKey, note->keyIndex());
                    highestKey = qMax(highestKey, note->keyIndex());
                }
                const auto keyCount = qMax(1, highestKey - lowestKey + 1);
                const auto noteHeight = qMin(10.0, previewBounds.height() / keyCount);
                for (const auto *note : singingClip->notes()) {
                    const auto noteGlobalStart = start + note->localStart();
                    const auto noteGlobalEnd = noteGlobalStart + note->length();
                    const auto visibleStart = start + clipStart;
                    const auto visibleEnd = visibleStart + clipLen;
                    if (noteGlobalEnd <= visibleStart || noteGlobalStart >= visibleEnd)
                        continue;
                    const auto noteLeft = sceneXForTick(qMax(noteGlobalStart, visibleStart));
                    const auto noteRight = sceneXForTick(qMin(noteGlobalEnd, visibleEnd));
                    const auto noteTop =
                        previewBounds.top() + (highestKey - note->keyIndex()) * noteHeight;
                    EditorRenderRect previewNote;
                    previewNote.bounds = {noteLeft, noteTop, qMax(1.0, noteRight - noteLeft),
                                          qMax(1.0, noteHeight - 1.0)};
                    previewNote.fill = previewColor;
                    previewNote.layer = 11;
                    snapshot->rectangles.append(previewNote);
                }
            } else if (previewBounds.height() > 4.0 && clip->clipType() == Clip::Audio) {
                const auto *audioClip = static_cast<const AudioClip *>(clip);
                appendAudioWaveform(*snapshot, audioClip, previewBounds, start, previewColor);
                if (audioClip->pathStatus() == AudioClip::PathStatus::Missing) {
                    snapshot->texts.append({
                        .text = tr("Missing audio file"),
                        .baseline = {clipRect.bounds.left() + 6.0, clipRect.bounds.bottom() - 7.0},
                        .color = QColor(255, 120, 120),
                        .font = clipFont,
                        .clip = clipRect.bounds.adjusted(4.0, 2.0, -4.0, -2.0),
                        .layer = 12,
                    });
                }
            }
        }
    }

    const auto quarterWidth = static_cast<double>(TracksEditorGlobal::pixelsPerQuarterNote);
    const auto firstGridX =
        std::floor(sceneXForTick(m_cachedStartTick) / quarterWidth) * quarterWidth;
    const auto lastGridX = qMin(snapshot->logicalExtent.width(), sceneXForTick(m_cachedEndTick));
    for (double x = firstGridX; x <= lastGridX; x += quarterWidth) {
        const auto quarter = qRound64(x / quarterWidth);
        snapshot->lines.append({
            {x, 0.0                             },
            {x, snapshot->logicalExtent.height()},
            quarter % 4 == 0 ? QColor(70, 75, 86) : QColor(47, 51, 60),
            1.0F,
            1
        });
    }
    if (m_interaction == Interaction::RectSelect && !m_selectionRect.isNull()) {
        EditorRenderRect selectionRect;
        selectionRect.bounds = m_selectionRect;
        selectionRect.fill = QColor(90, 145, 230, 45);
        selectionRect.border = QColor(120, 175, 255, 210);
        selectionRect.layer = 30;
        snapshot->rectangles.append(selectionRect);
    }
    snapshot->rectangles.append(m_pastePreviewRects);
    return snapshot;
}

void RhiTrackCanvas::publishSnapshot(const EditorDirtyDomains domains) {
    if (domains == EditorDirtyDomain::Camera && m_widget->snapshot()) {
        m_widget->setSnapshot(m_widget->snapshot(), domains);
        return;
    }
    QElapsedTimer timer;
    timer.start();
    const auto snapshot = buildSnapshot();
    m_widget->setSnapshotBuildDuration(timer.nsecsElapsed());
    m_revision = snapshot->revision;
    m_widget->setSnapshot(snapshot, domains);
}

void RhiTrackCanvas::ensureVisibleSnapshot() {
    const auto viewport = m_widget->viewportState();
    if (!m_widget->snapshot() || viewport.startTick < m_cachedStartTick ||
        viewport.endTick > m_cachedEndTick || viewport.topValue < m_cachedTopTrack ||
        viewport.bottomValue > m_cachedBottomTrack) {
        refreshSnapshot(EditorDirtyDomain::Geometry);
    }
}
