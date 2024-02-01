//
// Created by fluty on 2024/1/31.
//

#ifndef TRACKSVIEWCONTROLLER_H
#define TRACKSVIEWCONTROLLER_H

#include <QObject>

#include "Utils/Singleton.h"
#include "Model/DsTrackControl.h"

class TracksViewController final : public QObject, public Singleton<TracksViewController>{
    Q_OBJECT

public slots:
    void onNewTrack();
    void onInsertNewTrack(int index);
    void onRemoveTrack(int index);
    void addAudioClipToNewTrack(const QString &filePath);
    void onSelectedClipChanged(int trackIndex, int clipIndex);
    void onTrackPropertyChanged(const QString &name, const DsTrackControl &control, int index);
    void onAddAudioClip(const QString &path, int index);
};

#endif //TRACKSVIEWCONTROLLER_H
