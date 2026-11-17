#include "tst_audio_assets.h"

#include <QtTest>

#include <TalcsCore/AudioBuffer.h>
#include <TalcsCore/PositionableAudioSource.h>
#include <TalcsCore/TransportAudioSource.h>

namespace {
    // Fills the destination with its own absolute sample index, so a jump in the
    // source position is visible in the rendered samples.
    class RampSource final : public talcs::PositionableAudioSource {
    public:
        explicit RampSource(qint64 length) : m_length(length) {
        }

        qint64 length() const override {
            return m_length;
        }

        void setNextReadPosition(qint64 pos) override {
            m_position = pos;
        }

        qint64 nextReadPosition() const override {
            return m_position;
        }

    protected:
        qint64 processReading(const talcs::AudioSourceReadData &readData) override {
            const auto count = qBound(qint64(0), m_length - m_position, readData.length);
            for (int channel = 0; channel < readData.buffer->channelCount(); ++channel) {
                for (qint64 i = 0; i < count; ++i) {
                    readData.buffer->setSample(channel, readData.startPos + i,
                                               static_cast<float>(m_position + i));
                }
            }
            m_position += count;
            return count;
        }

    private:
        qint64 m_length;
        qint64 m_position = 0;
    };
}

void AudioAssetsTests::transportLoopWrapKeepsTheNextBlockAligned() {
    RampSource source(10000);
    talcs::TransportAudioSource transport;
    transport.setSource(&source);
    QVERIFY(transport.open(256, 48000));
    transport.setLoopingRange(100, 200);
    transport.setPosition(90);
    transport.play();

    talcs::AudioBuffer buffer(1, 256);
    talcs::AudioSourceReadData readData(&buffer, 0, 256);
    QCOMPARE(transport.read(readData), qint64{256});
    // The block spans the loop end once: 90..99, then 100..199 twice, then 100..145.
    QCOMPARE(buffer.sample(0, 0), 90.0f);
    QCOMPARE(buffer.sample(0, 9), 99.0f);
    QCOMPARE(buffer.sample(0, 10), 100.0f);
    QCOMPARE(buffer.sample(0, 109), 199.0f);
    QCOMPARE(buffer.sample(0, 110), 100.0f);
    QCOMPARE(buffer.sample(0, 209), 199.0f);
    QCOMPARE(buffer.sample(0, 210), 100.0f);
    QCOMPARE(buffer.sample(0, 255), 145.0f);
    // Only the samples read after the wrap advance the transport position.
    QCOMPARE(transport.position(), qint64{146});

    talcs::AudioBuffer next(1, 32);
    talcs::AudioSourceReadData nextReadData(&next, 0, 32);
    QCOMPARE(transport.read(nextReadData), qint64{32});
    QCOMPARE(next.sample(0, 0), 146.0f);
    QCOMPARE(next.sample(0, 31), 177.0f);
    QCOMPARE(transport.position(), qint64{178});
}
