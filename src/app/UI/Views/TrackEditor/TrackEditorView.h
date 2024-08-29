//
// Created by fluty on 2024/1/29.
//

#ifndef TRACKSVIEW_H
#define TRACKSVIEW_H

#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/Track.h"
#include "Model/AppModel/Clip.h"
#include "UI/Views/Common/PanelView.h"

class TrackListView;
class TracksGraphicsView;
class TracksGraphicsScene;
class TimelineView;
class TracksBackgroundGraphicsItem;
class TrackViewModel;
class AbstractClipView;

class TrackEditorView final : public PanelView {
    Q_OBJECT

public:
    explicit TrackEditorView(QWidget *parent = nullptr);

    AbstractClipView *findClipItemById(int id);

public slots:
    void onModelChanged();
    void onTrackChanged(AppModel::TrackChangeType type, int index);
    void onClipChanged(Track::ClipChangeType type, Clip *clip);
    void onPositionChanged(double tick);
    void onLastPositionChanged(double tick);
    void onLevelMetersUpdated(const AppModel::LevelMetersUpdatedArgs &args) const;

signals:
    void tempoChanged(double tempo);
    void trackCountChanged(qsizetype count);
    void setPositionTriggered(double tick);

private slots:
    // void onSceneSelectionChanged() const;
    void onViewScaleChanged(qreal sx, qreal sy) const;
    void onRemoveTrackTriggered(int id);

private:
    bool eventFilter(QObject *watched, QEvent *event) override;

    TrackListView *m_trackListView;
    TracksGraphicsView *m_graphicsView;
    TracksGraphicsScene *m_tracksScene;
    TimelineView *m_timeline;
    TracksBackgroundGraphicsItem *m_gridItem;

    class TrackListViewModel {
    public:
        QList<TrackViewModel *> tracks;

        TrackViewModel *findTrackById(int id);
    };

    TrackListViewModel m_trackListViewModel;

    void insertTrackToView(Track *dsTrack, int trackIndex);
    void insertClipToTrack(Clip *clip, TrackViewModel *track, int trackIndex);
    void insertSingingClip(SingingClip *clip, TrackViewModel *track, int trackIndex);
    void insertAudioClip(AudioClip *clip, TrackViewModel *track, int trackIndex);
    void removeClipFromView(int clipId);
    void updateTracksOnView() const;
    void updateClipOnView(Clip *clip);
    void removeTrackFromView(int index);
    void updateOverlappedState();
    void reset();
};



#endif // TRACKSVIEW_H
