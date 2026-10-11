#pragma once

#include <TalcsFormat/AudioFormatIO.h>

#include <algorithm>

namespace TestSupport {
    class UnavailableAudioBackend final : public talcs::AudioFormatIO {
    public:
        bool open(OpenMode) override {
            return false;
        }
    };

    class IncompleteAudioBackend final : public talcs::AudioFormatIO {
    public:
        explicit IncompleteAudioBackend(qint64 finalRead) : m_finalRead(finalRead) {
        }

        bool open(OpenMode) override {
            return true;
        }

        double sampleRate() const override {
            return 48000;
        }

        int channelCount() const override {
            return 1;
        }

        qint64 length() const override {
            return 4800;
        }

        qint64 read(float *samples, qint64 requested) override {
            if (m_readOnce)
                return m_finalRead;
            m_readOnce = true;
            const auto frames = std::min(requested, length());
            std::fill_n(samples, frames, 0.125f);
            return frames;
        }

    private:
        qint64 m_finalRead;
        bool m_readOnce = false;
    };
}
