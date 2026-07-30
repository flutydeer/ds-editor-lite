#include "LegacyTrackCanvasAdapter.h"

#include "AppContext.h"
#include "Global/TracksEditorGlobal.h"
#include "GraphicsItem/AudioClipView.h"
#include "GraphicsItem/SingingClipView.h"
#include "GraphicsItem/TrackEditorBackgroundView.h"
#include "Model/AppStatus/AppStatus.h"
#include "TracksGraphicsScene.h"
#include "TracksGraphicsView.h"
#include "UI/Utils/SpeakerMixDisplayUtils.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QHash>
#include <QMap>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTimer>

#include <cmath>

namespace {

    void updateSingingClipDisplay(SingingClip *clip, SingingClipView *view) {
        view->setSingerName(clip->singerInfo().name());
        view->setSpeakerName(SpeakerMixDisplayUtils::speakerDisplayName(
            clip->singerInfo(), clip->speakerInfo(), clip->speakerMixData()));
    }

    void applyAudioPathStatus(AudioClipView *clipView, const AudioClip::PathStatus status) {
        if (status == AudioClip::PathStatus::Missing) {
            clipView->setStatus(AppGlobal::Error);
            clipView->setErrorMessage({});
        } else {
            clipView->setStatus(AppGlobal::Loaded);
        }
    }

} // namespace

class LegacyTrackCanvasAdapter::Private final {
public:
    explicit Private(LegacyTrackCanvasAdapter *adapter) : q(adapter) {
    }

    ~Private() {
        qDeleteAll(pendingClipViews);
    }

    void initializeModelBridge() {
        connect(appModel, &AppModel::modelChanged, q, [this] { rebuildModel(); });
        connect(appModel, &AppModel::trackChanged, q,
                [this](const AppModel::TrackChangeType type, const qsizetype index, Track *track) {
                    if (type == AppModel::Insert)
                        insertTrack(track, index);
                    else if (type == AppModel::Remove)
                        removeTrack(track);
                    updateTrackIndexes();
                });
        connect(appModel, &AppModel::trackMoved, q,
                [this](const qsizetype, const qsizetype) { updateTrackIndexes(); });
        connect(appStatus, &AppStatus::selectedTrackIndexChanged, grid,
                &TrackEditorBackgroundView::onTrackSelectionChanged);
        connect(appStatus, &AppStatus::clipSelectionChanged, q, [this] { syncSelection(); });
        rebuildModel();
    }

    void rebuildModel() {
        for (auto trackIt = clipViews.begin(); trackIt != clipViews.end(); ++trackIt) {
            disconnect(trackIt.key(), nullptr, q, nullptr);
            for (auto *clip : trackIt.key()->clips())
                disconnect(clip, nullptr, q, nullptr);
            for (auto *item : std::as_const(trackIt.value())) {
                scene->removeCommonItem(item);
                delete item;
            }
        }
        clipViews.clear();
        qDeleteAll(pendingClipViews);
        pendingClipViews.clear();

        const auto tracks = appModel->tracks();
        for (qsizetype index = 0; index < tracks.size(); ++index)
            insertTrack(tracks.at(index), index);
        updateTrackIndexes();
        syncSelection();
    }

    void insertTrack(Track *track, const qsizetype index) {
        if (!track || clipViews.contains(track))
            return;
        clipViews.insert(track, {});
        connect(track, &Track::propertyChanged, q, [this] { updateTrackStyles(); });
        connect(track, &Track::clipChanged, q,
                [this, track](const Track::ClipChangeType type, Clip *clip) {
                    int trackIndex = -1;
                    appModel->findTrackById(track->id(), trackIndex);
                    if (type == Track::Inserted)
                        insertClip(clip, track, trackIndex);
                    else if (type == Track::Removed)
                        removeClip(clip, track);
                });
        for (auto *clip : track->clips())
            insertClip(clip, track, static_cast<int>(index));
    }

    void removeTrack(Track *track) {
        if (!track || !clipViews.contains(track))
            return;
        disconnect(track, nullptr, q, nullptr);
        const auto clips = clipViews.value(track).keys();
        for (auto *clip : clips)
            removeClip(clip, track);
        clipViews.remove(track);
    }

    void insertClip(Clip *clip, Track *track, const int trackIndex) {
        if (!clip || !track || trackIndex < 0)
            return;

        auto *clipView = pendingClipViews.take(clip->id());
        if (clipView) {
            clipView->setTrackIndex(trackIndex);
            clipView->setColorIndex(track->colorIndex());
            scene->addCommonItem(clipView);
        } else if (clip->clipType() == Clip::Singing) {
            auto *singingClip = static_cast<SingingClip *>(clip);
            auto *singingView = new SingingClipView(clip->id());
            singingView->loadCommonProperties(Clip::ClipCommonProperties(*clip));
            singingView->setTrackIndex(trackIndex);
            singingView->setColorIndex(track->colorIndex());
            singingView->loadNotes(singingClip->notes());
            updateSingingClipDisplay(singingClip, singingView);
            singingView->setDefaultLanguage(singingClip->defaultLanguage());
            scene->addCommonItem(singingView);
            clipView = singingView;
        } else if (clip->clipType() == Clip::Audio) {
            auto *audioClip = static_cast<AudioClip *>(clip);
            auto *audioView = new AudioClipView(clip->id());
            audioView->loadCommonProperties(Clip::ClipCommonProperties(*clip));
            audioView->setTrackIndex(trackIndex);
            audioView->setColorIndex(track->colorIndex());
            audioView->setPath(audioClip->path());
            audioView->setTimeline(appModel->timeline());
            audioView->setAudioInfo(audioClip->audioInfo());
            applyAudioPathStatus(audioView, audioClip->pathStatus());
            scene->addCommonItem(audioView);
            clipView = audioView;
        }

        if (!clipView)
            return;
        clipViews[track].insert(clip, clipView);
        connectClip(clip, clipView);
        syncSelection();
    }

    void connectClip(Clip *clip, AbstractClipView *view) {
        connect(clip, &Clip::propertyChanged, q, [this, clip] { updateClip(clip); });
        if (clip->clipType() == Clip::Singing) {
            auto *singingClip = static_cast<SingingClip *>(clip);
            auto *singingView = static_cast<SingingClipView *>(view);
            connect(singingClip, &SingingClip::voiceContextChanged, q,
                    [singingClip, singingView](const VoiceContextChange &) {
                        updateSingingClipDisplay(singingClip, singingView);
                    });
            connect(singingClip, &SingingClip::defaultLanguageChanged, singingView,
                    &SingingClipView::setDefaultLanguage);
            connect(singingClip, &SingingClip::noteChanged, singingView,
                    &SingingClipView::onNoteListChanged);
        } else if (clip->clipType() == Clip::Audio) {
            auto *audioClip = static_cast<AudioClip *>(clip);
            auto *audioView = static_cast<AudioClipView *>(view);
            connect(appModel, &AppModel::timelineChanged, audioView,
                    [audioView] { audioView->setTimeline(appModel->timeline()); });
            connect(audioClip, &AudioClip::pathStatusChanged, audioView,
                    [audioView](const AudioClip::PathStatus status) {
                        applyAudioPathStatus(audioView, status);
                    });
        }
    }

    void removeClip(Clip *clip, Track *track) {
        if (!clip || !track)
            return;
        disconnect(clip, nullptr, q, nullptr);
        auto *clipView = clipViews.value(track).take(clip);
        if (!clipView)
            return;
        scene->removeCommonItem(clipView);
        pendingClipViews.insert(clip->id(), clipView);
        QTimer::singleShot(0, q, [this, clipId = clip->id()] {
            if (auto *unusedView = pendingClipViews.take(clipId))
                delete unusedView;
        });
    }

    void updateClip(Clip *clip) {
        auto *item = findClipItemById(clip->id());
        if (!item)
            return;
        item->setName(clip->name());
        item->setStart(clip->start());
        item->setClipStart(clip->clipStart());
        item->setLength(clip->length());
        item->setClipLen(clip->clipLen());
        if (clip->clipType() == Clip::Audio) {
            auto *audioClip = static_cast<AudioClip *>(clip);
            auto *audioItem = static_cast<AudioClipView *>(item);
            audioItem->setPath(audioClip->path());
            audioItem->setAudioInfo(audioClip->audioInfo());
        } else if (clip->clipType() == Clip::Singing) {
            auto *singingClip = static_cast<SingingClip *>(clip);
            static_cast<SingingClipView *>(item)->loadNotes(singingClip->notes());
        }
    }

    void updateTrackStyles() {
        for (auto trackIt = clipViews.cbegin(); trackIt != clipViews.cend(); ++trackIt) {
            for (auto *clipView : trackIt.value())
                clipView->setColorIndex(trackIt.key()->colorIndex());
        }
        view->viewport()->update();
    }

    void updateTrackIndexes() {
        const auto tracks = appModel->tracks();
        for (qsizetype index = 0; index < tracks.size(); ++index) {
            for (auto *clipView : clipViews.value(tracks.at(index)))
                clipView->setTrackIndex(static_cast<int>(index));
        }
        const auto count = static_cast<int>(tracks.size());
        scene->onTrackCountChanged(count);
        grid->onTrackCountChanged(count);
        view->viewport()->update();
    }

    AbstractClipView *findClipItemById(const int id) const {
        for (const auto &track : clipViews)
            for (auto *clip : track)
                if (clip->id() == id)
                    return clip;
        return nullptr;
    }

    void syncSelection() const {
        const QSignalBlocker blocker(scene);
        const auto selected = appStatus->selectedClips.get();
        for (const auto &track : clipViews)
            for (auto *clip : track)
                clip->setSelected(selected.contains(clip->id()));
    }

    LegacyTrackCanvasAdapter *q = nullptr;
    TracksGraphicsScene *scene = nullptr;
    TracksGraphicsView *view = nullptr;
    TrackEditorBackgroundView *grid = nullptr;
    QHash<Track *, QHash<Clip *, AbstractClipView *>> clipViews;
    QMap<int, AbstractClipView *> pendingClipViews;
};

LegacyTrackCanvasAdapter::LegacyTrackCanvasAdapter(QObject *parent)
    : ITrackEditorCanvas(parent), d(std::make_unique<Private>(this)) {
    d->scene = new TracksGraphicsScene;
    d->scene->setParent(this);
    d->view = new TracksGraphicsView(d->scene);
    d->view->centerOn(0, 0);
    d->grid = new TrackEditorBackgroundView;
    d->grid->setPixelsPerQuarterNote(TracksEditorGlobal::pixelsPerQuarterNote);
    d->grid->setQuantize(128);
    d->view->setGridItem(d->grid);
    d->view->setSnapGrid(d->grid);

    connect(d->view, &TracksGraphicsView::scaleChanged, this, &ITrackEditorCanvas::scaleChanged);
    connect(d->view, &TracksGraphicsView::timeRangeChanged, this,
            &ITrackEditorCanvas::timeRangeChanged);
    connect(d->view, &TracksGraphicsView::visibleRectChanged, this,
            &ITrackEditorCanvas::visibleRectChanged);
    connect(d->view, &TracksGraphicsView::sizeChanged, this, &ITrackEditorCanvas::sizeChanged);
    connect(d->view, &TracksGraphicsView::sizeChanged, d->scene,
            &TracksGraphicsScene::onViewResized);
    d->initializeModelBridge();
}

LegacyTrackCanvasAdapter::~LegacyTrackCanvasAdapter() = default;

EditorCanvasBackend LegacyTrackCanvasAdapter::backend() const {
    return EditorCanvasBackend::Legacy;
}

QWidget *LegacyTrackCanvasAdapter::widget() const {
    return d->view;
}

QScrollBar *LegacyTrackCanvasAdapter::horizontalScrollBar() const {
    return d->view->horizontalScrollBar();
}

QScrollBar *LegacyTrackCanvasAdapter::verticalScrollBar() const {
    return d->view->verticalScrollBar();
}

EditorViewportState LegacyTrackCanvasAdapter::viewportState() const {
    const auto scaleY = d->view->scaleY();
    const auto trackHeight = TracksEditorGlobal::trackHeight * scaleY;
    EditorViewportState state;
    state.centerTick = (d->view->startTick() + d->view->endTick()) * 0.5;
    state.centerTrack =
        trackHeight > 0.0 ? d->view->visibleRect().center().y() / trackHeight - 0.5 : 0.0;
    state.horizontalScale = d->view->scaleX();
    state.verticalScale = scaleY;
    state.startTick = d->view->startTick();
    state.endTick = d->view->endTick();
    state.viewportSize = d->view->viewport()->size();
    state.devicePixelRatio = d->view->devicePixelRatioF();
    return state;
}

void LegacyTrackCanvasAdapter::restoreViewportState(const EditorViewportState &state) {
    if (!setViewportScale(state.horizontalScale, state.verticalScale))
        return;
    centerAt(state.centerTick, state.centerTrack);
    setPlaybackPosition(state.playbackPosition);
    setLastPlaybackPosition(state.lastPlaybackPosition);
}

bool LegacyTrackCanvasAdapter::centerAt(const double tick, const double trackIndex) {
    if (!std::isfinite(tick) || !std::isfinite(trackIndex))
        return false;
    d->view->stopViewportAnimations();
    const auto trackHeight = TracksEditorGlobal::trackHeight * d->view->scaleY();
    d->view->centerOn(d->view->sceneXForTick(tick), (trackIndex + 0.5) * trackHeight);
    d->view->notifyVisibleRectChanged();
    return true;
}

bool LegacyTrackCanvasAdapter::setViewportScale(const double horizontalScale,
                                                const double verticalScale) {
    return d->view->setViewportScale(horizontalScale, verticalScale);
}

QRectF LegacyTrackCanvasAdapter::visibleRect() const {
    return d->view->visibleRect();
}

double LegacyTrackCanvasAdapter::sceneXForTick(const double tick) const {
    return d->view->sceneXForTick(tick);
}

void LegacyTrackCanvasAdapter::setSceneLength(const int tick) {
    d->view->setSceneLength(tick);
}

void LegacyTrackCanvasAdapter::setPlaybackPosition(const double tick) {
    d->view->setPlaybackPosition(tick);
}

void LegacyTrackCanvasAdapter::setLastPlaybackPosition(const double tick) {
    d->view->setLastPlaybackPosition(tick);
}

void LegacyTrackCanvasAdapter::refreshSnapshot(const EditorDirtyDomains domains) {
    Q_UNUSED(domains);
    d->view->viewport()->update();
}

HistoryFocusVisibility LegacyTrackCanvasAdapter::focusVisibility(const HistoryFocus &focus) const {
    if (focus.kind != HistoryFocusKind::TrackClips || !focus.isValid())
        return HistoryFocusVisibility::Unavailable;

    QRectF itemBounds;
    for (const auto id : focus.objectIds) {
        if (const auto *item = d->findClipItemById(id))
            itemBounds = itemBounds.isNull() ? item->sceneBoundingRect()
                                             : itemBounds.united(item->sceneBoundingRect());
    }
    if (!itemBounds.isNull())
        return d->view->logicalVisibleRect().intersects(itemBounds)
                   ? HistoryFocusVisibility::Visible
                   : HistoryFocusVisibility::ScrollRequired;

    int trackIndex = focus.trackIndex;
    if (focus.trackId >= 0)
        appModel->findTrackById(focus.trackId, trackIndex);
    const auto visible = d->view->logicalVisibleRect();
    const auto left = d->view->sceneXForTick(focus.tickStart);
    const auto right = d->view->sceneXForTick(focus.tickEnd);
    const auto tickVisible = right >= visible.left() && left <= visible.right();
    const auto trackHeight = TracksEditorGlobal::trackHeight * d->view->scaleY();
    const auto trackTop = trackIndex * trackHeight;
    const auto trackBottom = trackTop + trackHeight;
    return tickVisible && trackBottom >= visible.top() && trackTop <= visible.bottom()
               ? HistoryFocusVisibility::Visible
               : HistoryFocusVisibility::ScrollRequired;
}

bool LegacyTrackCanvasAdapter::revealFocus(const HistoryFocus &focus, const bool animated) {
    if (focus.kind != HistoryFocusKind::TrackClips || !focus.isValid())
        return false;

    d->scene->clearSelection();
    QRectF itemBounds;
    for (const auto id : focus.objectIds) {
        if (auto *item = d->findClipItemById(id)) {
            item->setSelected(true);
            itemBounds = itemBounds.isNull() ? item->sceneBoundingRect()
                                             : itemBounds.united(item->sceneBoundingRect());
        }
    }
    if (!itemBounds.isNull()) {
        d->view->ensureSceneRectVisible(itemBounds, 24, 24, animated);
        return true;
    }

    int trackIndex = focus.trackIndex;
    if (focus.trackId >= 0 && !appModel->findTrackById(focus.trackId, trackIndex))
        return false;
    const auto trackHeight = TracksEditorGlobal::trackHeight * d->view->scaleY();
    const auto left = d->view->sceneXForTick(focus.tickStart);
    const auto right = d->view->sceneXForTick(focus.tickEnd);
    d->view->ensureSceneRectVisible(
        QRectF(left, trackIndex * trackHeight, qMax(1.0, right - left), trackHeight), 24, 24,
        animated);
    return true;
}

void LegacyTrackCanvasAdapter::onWheelHorScale(QWheelEvent *event) {
    d->view->onWheelHorScale(event);
}

void LegacyTrackCanvasAdapter::onWheelVerScale(QWheelEvent *event) {
    d->view->onWheelVerScale(event);
}

void LegacyTrackCanvasAdapter::onWheelHorScroll(QWheelEvent *event) {
    d->view->onWheelHorScroll(event);
}

void LegacyTrackCanvasAdapter::onWheelVerScroll(QWheelEvent *event) {
    d->view->onWheelVerScroll(event);
}
