//
// Created by fluty on 2024/1/29.
//

#ifndef TRACKSVIEW_H
#define TRACKSVIEW_H

#include <QWidget>

#include "Model/AppModel.h"
#include "Model/Track.h"
#include "Model/Clip.h"

class QListWidget;
class TracksGraphicsView;
class TracksGraphicsScene;
class TimelineView;
class TracksBackgroundGraphicsItem;
class TrackViewModel;
class AbstractClipGraphicsItem;

class TracksView final : public QWidget {
    Q_OBJECT

public:
    explicit TracksView(QWidget *parent = nullptr);

public slots:
    void onModelChanged();
    void onTempoChanged(double tempo);
    void onTrackChanged(AppModel::TrackChangeType type, int index);
    // void onPlaybackPositionChanged(long pos);
    // void onSamplerateChanged(int samplerate);
    void onClipChanged(Track::ClipChangeType type, qsizetype trackIndex, int clipIndex);
    void onPositionChanged(double tick);
    void onLastPositionChanged(double tick);
    void onLevelMetersUpdated(const AppModel::LevelMetersUpdatedArgs &args) const;

signals:
    void selectedClipChanged(int clipId);
    void trackPropertyChanged(const Track::TrackProperties &args);
    void insertNewTrackTriggered(int index);
    void removeTrackTriggerd(int index);
    void muteClicked(int index);
    void soloClicked(int index);
    void tempoChanged(double tempo);
    void trackCountChanged(qsizetype count);
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
    void updateTracksOnView() const;
    void updateClipOnView(Clip *clip, int clipId);
    void removeTrackFromView(int index);
    void updateOverlappedState(int trackIndex);
    void reset();
};



#endif // TRACKSVIEW_H
