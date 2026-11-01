#include "DecodeAudioTask.h"

#include <QCoreApplication>
#include <QDebug>
#include <QThread>

#include <TalcsFormat/AbstractAudioFormatIO.h>

#include <algorithm>
#include <vector>

DecodeAudioTask::DecodeAudioTask() {
    TaskStatus status;
    status.title = QCoreApplication::translate("DecodeAudioTask", "Decoding audio...");
    status.message = "";
    setStatus(status);
}

DecodeAudioTask::~DecodeAudioTask() {
    // The task owns the format IO handed to it by its creator
    // (AudioFilePreparer::createPrepareTask or the file dialog path).
    delete io;
}

AudioInfoModel DecodeAudioTask::result() const {
    AudioInfoModel info;
    info.sampleRate = m_sampleRate;
    info.channels = m_channels;
    info.chunkSize = m_chunkSize;
    info.mipmapScale = m_mipmapScale;
    info.frames = m_frames;
    info.peakCache = m_peakCache;
    info.peakCacheMipmap = m_peakCacheMipmap;
    info.peakCacheChannels = m_peakCacheChannels;
    info.peakCacheMipmapChannels = m_peakCacheMipmapChannels;
    return info;
}

void DecodeAudioTask::runTask() {
    TaskStatus status;
    status.title = QCoreApplication::translate("DecodeAudioTask", "Decoding audio...");
    status.message = path;
    setStatus(status);

    if (!io || !io->open(talcs::AbstractAudioFormatIO::Read)) {
        success = false;
        errorMessage = QCoreApplication::translate("DecodeAudioTask", "No audio IO");
        return;
    }

    QVector<std::tuple<short, short>> nullVector;
    m_peakCache.swap(nullVector);
    QVector<std::tuple<short, short>> nullVectorThumbnail;
    m_peakCacheMipmap.swap(nullVectorThumbnail);
    m_peakCacheChannels.clear();
    m_peakCacheMipmapChannels.clear();

    m_sampleRate = io->sampleRate();
    m_channels = io->channelCount();
    m_frames = io->length();

    std::vector<float> buffer(m_chunkSize * m_channels);
    std::vector<double> channelMax(m_channels, 0.0);
    std::vector<double> channelMin(m_channels, 0.0);
    const auto totalBufferCount = m_frames / m_chunkSize;
    long long buffersRead = 0;
    qint64 decodedFrames = 0;
    while (true) {
        if (isTerminateRequested()) {
            qDebug() << "Decode audio task abort:" << path;
            status.title = QCoreApplication::translate("DecodeAudioTask", "Canceling decoding...");
            status.isIndetermine = true;
            status.runningStatus = TaskGlobal::Error;
            setStatus(status);
            return;
        }
        const auto framesRead = io->read(buffer.data(), m_chunkSize);
        if (framesRead < 0 || (framesRead == 0 && decodedFrames < m_frames)) {
            success = false;
            errorMessage = QCoreApplication::translate("DecodeAudioTask", "Incomplete audio data");
            m_peakCache.clear();
            m_peakCacheMipmap.clear();
            m_peakCacheChannels.clear();
            m_peakCacheMipmapChannels.clear();
            return;
        }
        if (framesRead == 0) {
            break;
        }
        decodedFrames += framesRead;
        if (m_peakCacheChannels.size() != m_channels) {
            m_peakCacheChannels.resize(m_channels);
            m_peakCacheMipmapChannels.resize(m_channels);
        }
        std::fill(channelMax.begin(), channelMax.end(), 0.0);
        std::fill(channelMin.begin(), channelMin.end(), 0.0);
        double sampleMax = 0;
        double sampleMin = 0;
        for (qint64 i = 0; i < framesRead; i++) {
            double monoSample = 0.0;
            const auto frameOffset = i * m_channels;
            for (int j = 0; j < m_channels; j++) {
                const auto channelSample = static_cast<double>(buffer[frameOffset + j]);
                monoSample += channelSample / static_cast<double>(m_channels);
                // The audio view draws one lane per channel, so the same pass
                // also keeps the per-channel extremes
                if (channelSample > channelMax[j])
                    channelMax[j] = channelSample;
                if (channelSample < channelMin[j])
                    channelMin[j] = channelSample;
            }
            if (monoSample > sampleMax)
                sampleMax = monoSample;
            if (monoSample < sampleMin)
                sampleMin = monoSample;
        }

        auto toShortInt = [](double d) -> short {
            if (d < -1)
                d = -1;
            else if (d > 1)
                d = 1;
            return static_cast<short>(d * 32767);
        };

        short max = toShortInt(sampleMax);
        short min = toShortInt(sampleMin);

        auto pair = std::make_pair(min, max);
        m_peakCache.append(pair);
        for (int j = 0; j < m_channels; j++)
            m_peakCacheChannels[j].append(
                std::make_pair(toShortInt(channelMin[j]), toShortInt(channelMax[j])));
        buffersRead++;

        constexpr int updateProgressThreshold = 200;
        if (totalBufferCount < updateProgressThreshold) {
            status.progress = 100;
            setStatus(status);
        } else if (buffersRead % (totalBufferCount / 200) == 0) {
            const auto progress = 100 * buffersRead / totalBufferCount;
            status.progress = static_cast<int>(progress);
            setStatus(status);
        }
    }

    // Create mipmap from peak cache. Returns false when the task is being
    // terminated, the caller reports it the same way the read loop does.
    const auto buildMipmap = [this](const QVector<std::tuple<short, short>> &peaks,
                                    QVector<std::tuple<short, short>> &mipmap) {
        mipmap.clear();
        short min = 0;
        short max = 0;
        bool hasTail = false;
        for (int i = 0; i < peaks.count(); i++) {
            if ((i & 0xfff) == 0 && isTerminateRequested())
                return false;
            const auto [frameMin, frameMax] = peaks.at(i);
            min = std::min(min, frameMin);
            max = std::max(max, frameMax);
            hasTail = true;
            if ((i + 1) % m_mipmapScale == 0) {
                mipmap.append(std::make_pair(min, max));
                min = 0;
                max = 0;
                hasTail = false;
            }
        }
        if (hasTail)
            mipmap.append(std::make_pair(min, max));
        return true;
    };

    auto built = buildMipmap(m_peakCache, m_peakCacheMipmap);
    for (int channel = 0; built && channel < m_peakCacheChannels.size(); channel++)
        built = buildMipmap(m_peakCacheChannels[channel], m_peakCacheMipmapChannels[channel]);
    if (!built) {
        qDebug() << "Decode audio task abort:" << path;
        status.title = QCoreApplication::translate("DecodeAudioTask", "Canceling decoding...");
        status.isIndetermine = true;
        status.runningStatus = TaskGlobal::Error;
        setStatus(status);
        return;
    }

    success = true;
}
