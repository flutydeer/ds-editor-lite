#include "AnalysisAudio.h"

#include <algorithm>
#include <cmath>

#include <QCoreApplication>

#include <TalcsCore/AudioBuffer.h>
#include <TalcsCore/AudioSource.h>
#include <TalcsFormat/AbstractAudioFormatIO.h>
#include <TalcsFormat/AudioFormatInputSource.h>

namespace Extractors {

    namespace {

        constexpr qint64 CHUNK = 4096;

        QString tr(const char *text) {
            return QCoreApplication::translate("Extractors", text);
        }

    }

    std::optional<PreparedAudio> prepareAudio(talcs::AbstractAudioFormatIO *io, double startMs,
                                              double endMs, int sampleRate,
                                              const std::function<bool()> &cancelled,
                                              QString &error) {
        error.clear();
        if (io == nullptr) {
            error = tr("No audio IO");
            return std::nullopt;
        }
        if (sampleRate <= 0) {
            error = tr("The analyzer asked for an invalid sample rate");
            return std::nullopt;
        }
        if (!io->open(talcs::AbstractAudioFormatIO::Read)) {
            error = tr("Failed to open the audio file");
            return std::nullopt;
        }

        talcs::AudioFormatInputSource source(io, false);
        // The rate given to open() is the rate read() answers in, so the resampling happens here
        // and positions below are already counted in the analyzer's own frames.
        if (!source.open(CHUNK, sampleRate)) {
            error = tr("Failed to resample the audio to the rate the analyzer needs");
            return std::nullopt;
        }

        const auto channels = std::max(1, io->channelCount());
        const auto available = source.length();
        const auto toFrames = [sampleRate](double ms) {
            return static_cast<qint64>(std::llround(ms / 1000.0 * sampleRate));
        };
        const auto from = std::clamp<qint64>(toFrames(startMs), 0, available);
        const auto to = std::clamp<qint64>(toFrames(endMs), from, available);
        if (to == from) {
            error = tr("The selected region holds no audio");
            return std::nullopt;
        }

        PreparedAudio prepared;
        prepared.sampleRate = sampleRate;
        // Reported from the frame the read actually starts at rather than from what was asked
        // for. The two differ once the request is clamped, and an analyzer anchors its answer to
        // this number, so the difference would otherwise become a shift in the result.
        prepared.startMs = static_cast<double>(from) / sampleRate * 1000.0;
        prepared.samples.reserve(static_cast<std::size_t>(to - from));

        source.setNextReadPosition(from);
        talcs::AudioBuffer buffer(channels, CHUNK);
        for (auto at = from; at < to;) {
            if (cancelled && cancelled()) {
                return std::nullopt;
            }
            const auto wanted = std::min<qint64>(CHUNK, to - at);
            buffer.clear();
            const auto read = source.read(talcs::AudioSourceReadData(&buffer, 0, wanted));
            if (read <= 0) {
                break;
            }
            for (qint64 i = 0; i < read; ++i) {
                float sum = 0;
                for (int c = 0; c < channels; ++c) {
                    sum += buffer.constData(c)[i];
                }
                prepared.samples.push_back(sum / static_cast<float>(channels));
            }
            at += read;
        }

        if (prepared.samples.empty()) {
            error = tr("Failed to read the audio");
            return std::nullopt;
        }
        return prepared;
    }

}
