#include "DeviceTester.h"

#include <Modules/Audio/AudioSystem.h>
#include <Modules/Audio/subsystem/OutputSystem.h>
#include <TalcsCore/MixerAudioSource.h>
#include <TalcsDevice/AbstractOutputContext.h>

static DeviceTester *m_instance = nullptr;

DeviceTester::DeviceTester(QObject *parent) : QObject(parent) {
    m_instance = this;
    AudioSystem::outputSystem()->context()->preMixer()->addSource(this);
}

DeviceTester::~DeviceTester() {
    m_instance = nullptr;
}

void DeviceTester::playTestSound() {
    m_instance->startTest();
}

bool DeviceTester::open(const qint64 bufferSize, const double sampleRate) {
    static const double PI = std::acos(-1);
    m_sound.resize(1, static_cast<qint64>(sampleRate));
    double fadeIn = 0.005;
    double fadeOut = 0.005;
    const double rate = std::pow(0.99, 20000.0 / sampleRate);
    qint64 i, j;
    for (i = 0; i < m_sound.sampleCount() && fadeIn < 1.0; i++) {
        m_sound.sampleAt(0, i) = static_cast<float>(
            0.5 * std::sin(2.0 * PI * 440.0 / sampleRate * static_cast<double>(i)) * fadeIn);
        fadeIn /= rate;
    }
    for (j = m_sound.sampleCount() - 1; j >= 0 && fadeOut < 1.0; j--) {
        m_sound.sampleAt(0, j) = static_cast<float>(
            0.5 * std::sin(2.0 * PI * 440.0 / sampleRate * static_cast<double>(j)) * fadeOut);
        fadeOut /= rate;
    }
    for (; i <= j; i++) {
        m_sound.sampleAt(0, i) = static_cast<float>(
            0.5 * std::sin(2.0 * PI * 440.0 / sampleRate * static_cast<double>(i)));
    }
    m_pos = -1;
    return talcs::AudioSource::open(bufferSize, sampleRate);
}

void DeviceTester::close() {
    talcs::AudioSource::close();
}

void DeviceTester::startTest() {
    m_pos = 0;
}

qint64 DeviceTester::processReading(const talcs::AudioSourceReadData &readData) {
    const qint64 pos = m_pos;
    if (pos < 0)
        return readData.length;
    const qint64 length = qMin(readData.length, m_sound.sampleCount() - pos);
    for (int ch = 0; ch < readData.buffer->channelCount(); ch++) {
        readData.buffer->setSampleRange(ch, readData.startPos, length, m_sound, 0, pos);
        readData.buffer->clear(ch, readData.startPos + length, readData.length - length);
    }
    if (m_pos == pos)
        m_pos += length;
    return readData.length;
}