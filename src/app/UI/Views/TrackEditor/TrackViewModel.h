//
// Created by fluty on 2024/2/8.
//

#ifndef TRACKVIEWMODEL_H
#define TRACKVIEWMODEL_H

#include <QList>

class Track;
class TrackControlView;
class AbstractClipView;

class TrackViewModel final {
public:
    explicit TrackViewModel(Track *track) : dsTrack(track) {
    }

    Track *dsTrack;
    TrackControlView *controlView = nullptr;
    bool isSelected = false;
    QMap<Clip *,AbstractClipView *> clips = {};
};



#endif // TRACKVIEWMODEL_H
