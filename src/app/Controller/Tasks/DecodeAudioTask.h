//
// Created by fluty on 24-3-10.
//

#ifndef DECODEAUDIOTASK_H
#define DECODEAUDIOTASK_H

#include "Modules/Task/ITask.h"
#include "Utils/UniqueObject.h"

#include <sndfile.hh>

class DecodeAudioTask : public ITask, public UniqueObject {
public:
    explicit DecodeAudioTask() = default;
    explicit DecodeAudioTask(int id) : UniqueObject(id){}

    int trackIndex = 0;
    int tick = 0;
    QString path;
    int sampleRate = 0;
    int channels = 0;
    long long frames = 0;
    int chunkSize = 512;
    int mipmapScale = 10;
    QVector<std::tuple<short, short>> peakCache;
    QVector<std::tuple<short, short>> peakCacheMipmap;
    bool success = false;
    QString errorMessage;

private:
    void runTask() override;

    SndfileHandle sf;
};

#endif // DECODEAUDIOTASK_H
