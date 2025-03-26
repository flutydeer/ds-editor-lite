//
// Created by fluty on 2024/1/29.
//

#ifndef TRACKSVIEW_H
#define TRACKSVIEW_H

#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/Track.h"
#include "UI/Views/Common/PanelView.h"


class AudioClip;
class SingingClip;
class TrackListView;
class TracksGraphicsView;
class TracksGraphicsScene;
class TimelineView;
class TrackEditorBackgroundView;
class TrackViewModel;
class AbstractClipView;
class ChannelView;

class TrackEditorView final : public PanelView {
    Q_OBJECT

public:
    explicit TrackEditorView(QWidget *parent = nullptr);

    AbstractClipView *findClipItemById(int id);

public slots:
    void onModelChanged();
    void onTrackChanged(AppModel::TrackChangeType type, qsizetype index, Track *track);
    void onClipChanged(Track::ClipChangeType type, Clip *clip, Track *dsTrack);
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
    static void onRemoveTrackTriggered(int id);

private:
    bool eventFilter(QObject *watched, QEvent *event) override;

    TrackListView *m_trackListView;
    TracksGraphicsView *m_graphicsView;
    TracksGraphicsScene *m_tracksScene;
    TimelineView *m_timeline;
    TrackEditorBackgroundView *m_gridItem;

    ChannelView *masterChannel;

    class ViewModel {
    public:
        QList<TrackViewModel *> tracks;
        TrackViewModel *findTrack(Track *dsTrack);
    };

    ViewModel m_viewModel;

    void onTrackInserted(Track *dsTrack, qsizetype trackIndex);
    void onClipInserted(Clip *clip, TrackViewModel *track, int trackIndex);
    void insertSingingClip(SingingClip *clip, TrackViewModel *track, int trackIndex);
    void insertAudioClip(AudioClip *clip, TrackViewModel *track, int trackIndex);
    void onClipRemoved(Clip *clip, TrackViewModel *track);
    void onTrackPropertyChanged() const;
    void updateClipOnView(Clip *clip);
    void onTrackRemoved(Track *dsTrack, qsizetype index);
};



#endif // TRACKSVIEW_H
