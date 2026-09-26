#pragma once

#include <TalcsFormat/AudioFormatIO.h>

#include <QFile>
#include <QVector>

namespace TestSupport {
    inline bool writeWave(const QString &path, const QVector<float> &samples, int channelCount = 1,
                          int sampleRate = 48000) {
        if (channelCount <= 0 || samples.size() % channelCount != 0)
            return false;
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return false;
        talcs::AudioFormatIO writer(&file);
        writer.setSampleRate(sampleRate);
        writer.setChannelCount(channelCount);
        writer.setFormat(talcs::AudioFormatIO::WAV | talcs::AudioFormatIO::FLOAT);
        if (!writer.open(talcs::AbstractAudioFormatIO::Write))
            return false;
        const auto frameCount = samples.size() / channelCount;
        return writer.write(samples.constData(), frameCount) == frameCount;
    }
}
