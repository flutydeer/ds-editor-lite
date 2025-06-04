//
// Created by FlutyDeer on 2025/6/3.
//

#ifndef MIXCONSOLEVIEW_H
#define MIXCONSOLEVIEW_H

#include "Model/AppModel/AppModel.h"

#include <QWidget>

class Track;
class QListWidget;
class ChannelView;

class MixConsoleView : public QWidget {
    Q_OBJECT

public:
    explicit MixConsoleView(QWidget *parent = nullptr);

public slots:
    void onModelChanged();
    void onTrackChanged(AppModel::TrackChangeType type, qsizetype index, Track *track);
    void onMasterControlChanged(const TrackControl &control);
    void onLevelMetersUpdated(const AppModel::LevelMetersUpdatedArgs &args) const;

private slots:
    void onTrackInserted(Track *dsTrack, qsizetype trackIndex);
    void onTrackRemoved(qsizetype index);

private:
    void onTrackPropertyChanged() const;

    QListWidget *m_channelListView;

    ChannelView *m_masterChannel;
};


#endif //MIXCONSOLEVIEW_H