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
    // One peak cache per channel, filled by the same decode pass that builds
    // the downmixed caches above. The track editor keeps drawing the downmix,
    // the audio view draws one lane per channel from these. Empty means the
    // caches were not built (older documents, hand-built info in tests) and
    // callers must fall back to the downmixed caches.
    QList<QList<std::tuple<short, short>>> peakCacheChannels;
    QList<QList<std::tuple<short, short>>> peakCacheMipmapChannels;
};



#endif // AUDIOINFOMODEL_H
