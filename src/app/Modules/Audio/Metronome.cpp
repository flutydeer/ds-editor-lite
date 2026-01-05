//
// Created by Kuro on 2026/1/5.
//

#include "Metronome.h"

#include <QtMath>

Metronome::Metronome(QObject *parent) : QObject(parent) {
}

Metronome::~Metronome() = default;

bool Metronome::open(qint64 bufferSize, double sampleRate) {
    m_sampleRate = sampleRate;
    m_clickPos = -1;
    m_lastBeatIndex = -1;
    return talcs::AudioSource::open(bufferSize, sampleRate);
}

void Metronome::close() {
    talcs::AudioSource::close();
}

void Metronome::setEnabled(bool enabled) {
    if (m_enabled != enabled) {
        m_enabled = enabled;
        m_clickPos = -1;
        emit enabledChanged(enabled);
    }
}

bool Metronome::isEnabled() const {
    return m_enabled;
}

void Metronome::setGain(double gain) {
    if (!qFuzzyCompare(m_gain, gain)) {
        m_gain = gain;
        emit gainChanged(gain);
    }
}

double Metronome::gain() const {
    return m_gain;
}

void Metronome::setPlaying(bool playing) {
    if (m_playing != playing) {
        m_playing = playing;
        m_clickPos = -1;
    }
}

void Metronome::setTimeSignature(int numerator, int denominator) {
    m_numerator = numerator;
    m_denominator = denominator;
}

void Metronome::setTempo(double tempo) {
    m_tempo = tempo;
}

void Metronome::setPosition(qint64 positionSample) {
    m_position = positionSample;
    m_clickPos = -1;
    if (m_tempo > 0 && m_sampleRate > 0 && positionSample >= 0) {
        const double samplesPerBeat = 60.0 * m_sampleRate / m_tempo;
        const auto beatIndex = static_cast<qint64>(positionSample / samplesPerBeat);
        const auto beatStartSample = static_cast<qint64>(beatIndex * samplesPerBeat);
        // If position is at or very close to beat start (within 5ms), allow the beat to trigger
        const qint64 tolerance = static_cast<qint64>(m_sampleRate * 0.005);
        if (positionSample - beatStartSample <= tolerance) {
            m_lastBeatIndex = beatIndex - 1;
        } else {
            m_lastBeatIndex = beatIndex;
        }
    } else {
        m_lastBeatIndex = -1;
    }
}

qint64 Metronome::processReading(const talcs::AudioSourceReadData &readData) {
    for (int ch = 0; ch < readData.buffer->channelCount(); ++ch) {
        readData.buffer->clear(ch, readData.startPos, readData.length);
    }

    if (!m_enabled || !m_playing || m_tempo <= 0 || m_sampleRate <= 0) {
        emit levelMetered(0);
        return readData.length;
    }

    const double samplesPerBeat = 60.0 * m_sampleRate / m_tempo;
    const int clickDuration = static_cast<int>(m_sampleRate * 0.03); // 30ms click
    float peakLevel = 0;

    for (qint64 i = 0; i < readData.length; ++i) {
        const qint64 currentSample = m_position + i;
        if (currentSample < 0)
            continue;

        const auto beatIndex = static_cast<qint64>(currentSample / samplesPerBeat);

        // Detect new beat
        if (beatIndex != m_lastBeatIndex) {
            m_lastBeatIndex = beatIndex;
            const int beatInBar = static_cast<int>(beatIndex % m_numerator);
            m_isHighClick = (beatInBar == 0);
            m_clickPos = 0;
        }

        // Generate click sound
        if (m_clickPos >= 0 && m_clickPos < clickDuration) {
            const double freq = m_isHighClick ? 1000.0 : 800.0; // High: 1kHz, Low: 800Hz
            const double envelope = 1.0 - static_cast<double>(m_clickPos) / clickDuration; // Linear decay
            const double sample = qSin(2.0 * M_PI * freq * m_clickPos / m_sampleRate) * envelope * m_gain;
            const auto sampleF = static_cast<float>(sample);

            for (int ch = 0; ch < readData.buffer->channelCount(); ++ch) {
                readData.buffer->sampleAt(ch, readData.startPos + i) = sampleF;
            }
            peakLevel = qMax(peakLevel, qAbs(sampleF));
            ++m_clickPos;
        }
    }

    emit levelMetered(peakLevel);
    m_position += readData.length;
    return readData.length;
}
