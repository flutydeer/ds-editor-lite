//
// Created by fluty on 24-3-10.
//

#ifndef DECODEAUDIOTASK_H
#define DECODEAUDIOTASK_H

#include <QJsonObject>

#include "Model/AppModel/AudioInfoModel.h"
#include "Modules/Task/Task.h"

namespace talcs {
    class AbstractAudioFormatIO;
}

class DecodeAudioTask : public Task {
public:
    explicit DecodeAudioTask();
    // explicit DecodeAudioTask(int id);

    talcs::AbstractAudioFormatIO *io{};
    int trackId = -1;
    int clipId = -1;
    int tick = 0;
    QString path;
    bool success = false;
    QString errorMessage;
    QJsonObject workspace;
    [[nodiscard]] AudioInfoModel result() const;

private:
    void runTask() override;

    int m_sampleRate = 0;
    int m_channels = 0;
    long long m_frames = 0;
    int m_chunkSize = 512;
    int m_mipmapScale = 10;
    QVector<std::tuple<short, short>> m_peakCache;
    QVector<std::tuple<short, short>> m_peakCacheMipmap;
};

#endif // DECODEAUDIOTASK_H
