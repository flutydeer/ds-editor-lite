//
// Created by fluty on 2024/1/31.
//

#ifndef TRACKSVIEWCONTROLLER_H
#define TRACKSVIEWCONTROLLER_H

#include <QObject>

#include "Utils/Singleton.h"
#include "Model/DsClip.h"
#include "Model/DsTrack.h"
#include "Model/DsTrackControl.h"

class TracksViewController final : public QObject, public Singleton<TracksViewController> {
    Q_OBJECT

public slots:
    void onNewTrack();
    void onInsertNewTrack(int index);
    void onAppendTrack(DsTrack *track);
    void onRemoveTrack(int index);
    void addAudioClipToNewTrack(const QString &filePath);
    void onSelectedClipChanged(int trackIndex, int clipIndex);
    void onTrackPropertyChanged(const DsTrack::TrackProperties &args);
    void onAddAudioClip(const QString &path, int trackIndex, int tick);
    void onClipPropertyChanged(const DsClip::ClipCommonProperties &args);
    void onRemoveClip(int clipId);
    void onNewSingingClip(int trackIndex, int tick);
};

#endif // TRACKSVIEWCONTROLLER_H
