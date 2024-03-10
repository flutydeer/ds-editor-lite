//
// Created by fluty on 24-3-10.
//

#ifndef AUDIOINFOMODEL_H
#define AUDIOINFOMODEL_H

#include <QList>

class AudioInfoModel {
public:
    int chunkSize = 0;
    int mipmapScale = 0;
    int sampleRate = 0;
    int channels = 0;
    long long frames = 0;
    QList<std::tuple<short, short>> peakCache;
    QList<std::tuple<short, short>> peakCacheMipmap;
};



#endif // AUDIOINFOMODEL_H
