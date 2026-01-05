//
// Created by Kuro on 2025/1/5.
//

#ifndef METRONOME_H
#define METRONOME_H

#include <QObject>
#include <TalcsCore/AudioSource.h>

class Metronome : public QObject, public talcs::AudioSource {
    Q_OBJECT

public:
    explicit Metronome(QObject *parent = nullptr);
    ~Metronome() override;

    bool open(qint64 bufferSize, double sampleRate) override;
    void close() override;

    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const;

    void setGain(double gain);
    [[nodiscard]] double gain() const;

    void setPlaying(bool playing);

    void setTimeSignature(int numerator, int denominator);
    void setTempo(double tempo);
    void setPosition(qint64 positionSample);

signals:
    void enabledChanged(bool enabled);
    void gainChanged(double gain);
    void levelMetered(float level);

protected:
    qint64 processReading(const talcs::AudioSourceReadData &readData) override;

private:
    double m_sampleRate = 48000.0;
    bool m_enabled = false;
    bool m_playing = false;
    double m_gain = 1.0;

    int m_numerator = 4;
    int m_denominator = 4;
    double m_tempo = 120.0;

    qint64 m_position = 0;
    qint64 m_lastBeatIndex = -1;
    int m_clickPos = -1;
    bool m_isHighClick = false;
};

#endif // METRONOME_H
