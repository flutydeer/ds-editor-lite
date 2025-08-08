//
// Created by fluty on 24-3-10.
//

#include "DecodeAudioTask.h"

#include <QDebug>
#include <QThread>

#include <TalcsFormat/AbstractAudioFormatIO.h>

DecodeAudioTask::DecodeAudioTask(/*int id*/) /*: ITask(id)*/ {
    TaskStatus status;
    status.title = "Decoding audio...";
    status.message = "";
    setStatus(status);
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
    return info;
}

void DecodeAudioTask::runTask() {
    TaskStatus status;
    status.title = "Decoding audio...";
    status.message = path;
    setStatus(status);

    auto pathStr =
#ifdef Q_OS_WIN
        path.toStdWString();
#else
        path.toStdString();
#endif
    //    SndfileHandle sf(pathStr.c_str());

    if (!io || !io->open(talcs::AbstractAudioFormatIO::Read)) {
        success = false;
        errorMessage = "No io"; // TODO talcs::FormatEntry should provide error message
        return;
    }

    QVector<std::tuple<short, short>> nullVector;
    m_peakCache.swap(nullVector);
    QVector<std::tuple<short, short>> nullVectorThumbnail;
    m_peakCacheMipmap.swap(nullVectorThumbnail);

    m_sampleRate = io->sampleRate();
    m_channels = io->channelCount();
    m_frames = io->length();
    // auto totalSize = frames * channels;
    // qDebug() << frames;

    std::vector<float> buffer(m_chunkSize * m_channels);
    const auto totalBufferCount = m_frames / m_chunkSize;
    long long buffersRead = 0;
    qint64 samplesRead = 0;
    while (samplesRead < m_frames * m_channels) {
        if (isTerminateRequested()) {
            qDebug() << "Decode audio task abort:" << path;
            status.title = "Canceling decoding...";
            status.isIndetermine = true;
            status.runningStatus = TaskGlobal::Error;
            setStatus(status);
            // QThread::sleep(3);
            return;
        }
        samplesRead = io->read(buffer.data(), m_chunkSize);
        if (samplesRead == 0) {
            break;
        }
        double sampleMax = 0;
        double sampleMin = 0;
        const qint64 framesRead = samplesRead / m_channels;
        for (qint64 i = 0; i < framesRead; i++) {
            double monoSample = 0.0;
            for (int j = 0; j < m_channels; j++) {
                monoSample += buffer[i * m_channels + j] / static_cast<double>(m_channels);
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
        buffersRead++;

        constexpr int updateProgressThreshold = 200;
        if (totalBufferCount < updateProgressThreshold) {
            status.progress = 100;
            setStatus(status);
        } else if (buffersRead % (totalBufferCount / 200) == 0) {
            const auto progress = 100 * buffersRead / totalBufferCount;
            // qDebug() << progress;
            status.progress = static_cast<int>(progress);
            setStatus(status);
        }
        // QThread::msleep(1);
    }

    // Create mipmap from peak cache
    short min = 0;
    short max = 0;
    bool hasTail = false;
    for (int i = 0; i < m_peakCache.count(); i++) {
        if (isTerminateRequested()) {
            qDebug() << "Decode audio task abort:" << path;
            status.title = "Canceling decoding...";
            status.isIndetermine = true;
            status.runningStatus = TaskGlobal::Error;
            setStatus(status);
            // QThread::sleep(3);
            return;
        }
        if ((i + 1) % m_mipmapScale == 0) {
            m_peakCacheMipmap.append(std::make_pair(min, max));
            min = 0;
            max = 0;
            hasTail = false;
        } else {
            auto frame = m_peakCache.at(i);
            const auto frameMin = std::get<0>(frame);
            const auto frameMax = std::get<1>(frame);
            if (frameMin < min)
                min = frameMin;
            if (frameMax > max)
                max = frameMax;
            hasTail = true;
        }
    }
    if (hasTail)
        m_peakCacheMipmap.append(std::make_pair(min, max));

    // QThread::msleep(3000);
    success = true;
}