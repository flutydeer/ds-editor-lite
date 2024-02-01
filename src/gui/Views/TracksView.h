//
// Created by fluty on 2024/1/29.
//

#ifndef TRACKSVIEW_H
#define TRACKSVIEW_H

#include <QListWidget>

#include "Controls/TracksEditor/TracksGraphicsScene.h"
#include "Controls/TracksEditor/TracksGraphicsView.h"
#include "Controls/TracksEditor/TrackControlWidget.h"

class TracksView final : public QWidget {
    Q_OBJECT

public:
    explicit TracksView();

public slots:
    void onModelChanged(const AppModel &model);
    void onTempoChanged(double tempo);
    void onTrackChanged(AppModel::TrackChangeType type, const AppModel &model, int index);
    // void onPlaybackPositionChanged(long pos);
    // void onSamplerateChanged(int samplerate);

signals:
    void selectedClipChanged(int trackIndex, int clipIndex);
    void trackPropertyChanged(const QString &name, const DsTrackControl &control, int index);
    void insertNewTrackTriggered(int index);
    void removeTrackTriggerd(int index);
    void tempoChanged(double tempo);
    void trackCountChanged(int count);
    // void adjustTrackListItemHeight(int i);

private slots:
    void onSceneSelectionChanged();
    void onViewScaleChanged(qreal sx, qreal sy);

private:
    QListWidget *m_trackListWidget;
    TracksGraphicsView *m_graphicsView;
    TracksGraphicsScene *m_tracksScene;

    class Track {
    public:
        // widget
        TrackControlWidget *widget;
        // properties
        bool isSelected;
        // clips
        QList<AbstractClipGraphicsItem *> clips;
    };

    class TracksViewModel {
    public:
        QList<Track> tracks;
    };

    TracksViewModel m_tracksModel;
    double m_tempo = 120;
    int m_samplerate = 48000;
    int positionInTick = 1920;

    void insertTrackToView(const DsTrack &dsTrack, int index);
    void removeTrackFromView(int index);
    void reset();
};



#endif // TRACKSVIEW_H
