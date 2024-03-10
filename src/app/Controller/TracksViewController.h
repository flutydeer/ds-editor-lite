//
// Created by fluty on 2024/1/31.
//

#ifndef TRACKSVIEWCONTROLLER_H
#define TRACKSVIEWCONTROLLER_H

#include <QObject>

#include "Utils/Singleton.h"
#include "Model/Clip.h"
#include "Model/Track.h"
#include "Tasks/DecodeAudioTask.h"
#include "UI/Views/TracksEditor/TracksView.h"

class TracksViewController final : public QObject, public Singleton<TracksViewController> {
    Q_OBJECT

public slots:
    void onNewTrack();
    void onInsertNewTrack(qsizetype index);
    void onAppendTrack(Track *track);
    void onRemoveTrack(int index);
    void addAudioClipToNewTrack(const QString &filePath);
    void onSelectedClipChanged(int clipId);
    void onTrackPropertyChanged(const Track::TrackProperties &args);
    void onAddAudioClip(const QString &path, int trackIndex, int tick);
    void onClipPropertyChanged(const Clip::ClipCommonProperties &args);
    void onRemoveClip(int clipId);
    void onNewSingingClip(int trackIndex, int tick);

private:
    void handleDecodeAudioTaskFinished(DecodeAudioTask *task, bool terminate);

    TracksView m_view;
};

#endif // TRACKSVIEWCONTROLLER_H
