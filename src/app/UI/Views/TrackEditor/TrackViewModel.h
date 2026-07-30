//
// Created by fluty on 2024/2/8.
//

#ifndef TRACKVIEWMODEL_H
#define TRACKVIEWMODEL_H

class Track;
class TrackControlView;

class TrackViewModel final {
public:
    explicit TrackViewModel(Track *track) : dsTrack(track) {
    }

    Track *dsTrack;
    TrackControlView *controlView = nullptr;
    bool isSelected = false;
};



#endif // TRACKVIEWMODEL_H
