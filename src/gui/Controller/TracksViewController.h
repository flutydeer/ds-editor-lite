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

public:
    class ClipPropertyChangedArgs {
    public:
        QString name;
        int start = 0;
        int length = 0;
        int clipStart = 0;
        int clipLen = 0;
        double gain = 0;
        bool mute = false;

        int trackIndex = 0;
        int clipIndex = 0;
    };
    class AudioClipPropertyChangedArgs : public ClipPropertyChangedArgs {
    public:
        QString path;
    };

public slots:
    void onNewTrack();
    void onInsertNewTrack(int index);
    void onRemoveTrack(int index);
    void addAudioClipToNewTrack(const QString &filePath);
    void onSelectedClipChanged(int trackIndex, int clipIndex);
    void onTrackPropertyChanged(const QString &name, const DsTrackControl &control, int index);
    void onAddAudioClip(const QString &path, int index);
    void onClipPropertyChanged(const ClipPropertyChangedArgs &args);
};

#endif //TRACKSVIEWCONTROLLER_H
