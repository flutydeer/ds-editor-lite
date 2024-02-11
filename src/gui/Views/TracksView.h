//
// Created by fluty on 2024/1/29.
//

#ifndef TRACKSVIEW_H
#define TRACKSVIEW_H

#include <QListWidget>

#include "Controls/TracksEditor/TracksGraphicsScene.h"
#include "Controls/TracksEditor/TracksGraphicsView.h"
#include "Controls/TracksEditor/TrackControlWidget.h"
#include "Controls/TracksEditor/TracksBackgroundGraphicsItem.h"
#include "Controller/TracksViewController.h"
#include "Controls/Base/TimeIndicatorGraphicsItem.h"
#include "Controls/Base/TimelineView.h"
#include "Controls/TracksEditor/AbstractClipGraphicsItem.h"
#include "Controls/TracksEditor/TrackViewModel.h"

class TracksView final : public QWidget {
    Q_OBJECT

public:
    explicit TracksView();

public slots:
    void onModelChanged();
    void onTempoChanged(double tempo);
    void onTrackChanged(AppModel::TrackChangeType type, int index);
    // void onPlaybackPositionChanged(long pos);
    // void onSamplerateChanged(int samplerate);
    void onClipChanged(Track::ClipChangeType type, int trackIndex, int clipIndex);
    void onPositionChanged(double tick);
    void onLastPositionChanged(double tick);
    void onLevelMetersUpdated(const AppModel::LevelMetersUpdatedArgs &args);

signals:
    void selectedClipChanged(int clipId);
    void trackPropertyChanged(const Track::TrackProperties &args);
    void insertNewTrackTriggered(int index);
    void removeTrackTriggerd(int index);
    void muteClicked(int index);
    void soloClicked(int index);
    void tempoChanged(double tempo);
    void trackCountChanged(int count);
    void addAudioClipTriggered(const QString &path, int trackIndex, int tick);
    void clipPropertyChanged(const Clip::ClipCommonProperties &args);
    void setPositionTriggered(double tick);
    void removeClipTriggered(int clipId);
    void newSingingClipTriggered(int trackIndex, int tick);

private slots:
    void onSceneSelectionChanged();
    void onViewScaleChanged(qreal sx, qreal sy);

private:
    QListWidget *m_trackListWidget;
    TracksGraphicsView *m_graphicsView;
    TracksGraphicsScene *m_tracksScene;
    TimelineView *m_timeline;
    TracksBackgroundGraphicsItem *m_gridItem;

    class TrackListViewModel {
    public:
        QList<TrackViewModel *> tracks;
    };

    TrackListViewModel m_trackListViewModel;
    double m_tempo = 120;
    int m_samplerate = 48000;

    Track::ClipChangeType m_prevClipChangeType = Track::Removed;
    int m_prevClipId = -1;

    void insertTrackToView(Track *dsTrack, int trackIndex);
    void insertClipToTrack(Clip *clip, TrackViewModel *track, int trackIndex);
    void removeClipFromView(int clipId);
    AbstractClipGraphicsItem *findClipItemById(int id);
    void updateTracksOnView();
    void updateClipOnView(Clip *clip, int clipId);
    void removeTrackFromView(int index);
    void updateOverlappedState(int trackIndex);
    void reset();
};



#endif // TRACKSVIEW_H
