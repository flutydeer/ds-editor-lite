#include <audio-util/Resample.h>

#include <fstream>
#include <iostream>
#include <vector>

#include <CDSPResampler.h>

#include "FlacDecoder.h"
#include "Mp3Decoder.h"

namespace AudioUtil
{
    static void convert_to_mono(short *buffer, const sf_count_t frames, const int channels) {
        if (channels == 2) {
            for (sf_count_t i = 0; i < frames; ++i) {
                buffer[i] = static_cast<short>((buffer[2 * i] + buffer[2 * i + 1]) / 2);
            }
        }
    }

    static void write_wav_to_vio(const std::filesystem::path &filepath, SF_VIO &sf_vio) {
        SF_INFO sfinfo;
        SNDFILE *infile = sf_open(filepath.string().c_str(), SFM_READ, &sfinfo);
        if (!infile) {
            std::cerr << "Failed to open WAV file: " << filepath << std::endl;
            return;
        }

        SndfileHandle outBuf(sf_vio.vio, &sf_vio.data, SFM_WRITE, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 1,
                             sfinfo.samplerate);

        short buffer[1024];
        sf_count_t frames;
        while ((frames = sf_read_short(infile, buffer, sizeof(buffer) / 2)) > 0) {
            convert_to_mono(buffer, frames, sfinfo.channels);
            outBuf.write(buffer, frames);
        }
        sf_close(infile);
    }

    void write_audio_to_vio(const std::filesystem::path &filepath, SF_VIO &sf_vio) {
        const std::string extension = filepath.extension().string();
        if (extension == ".wav") {
            write_wav_to_vio(filepath, sf_vio);
        } else if (extension == ".mp3") {
            write_mp3_to_vio(filepath, sf_vio);
        } else if (extension == ".flac") {
            write_flac_to_vio(filepath, sf_vio);
        } else {
            std::cerr << "Unsupported file format: " << filepath << std::endl;
            exit(-1);
        }
        sf_vio.data.seek = 0;
    }

    SF_VIO resample(const std::filesystem::path &filepath, const int tar_channel, const int tar_samplerate) {
        SF_VIO sf_vio_in;
        write_audio_to_vio(filepath, sf_vio_in);

        SndfileHandle srcHandle(sf_vio_in.vio, &sf_vio_in.data, SFM_READ, SF_FORMAT_WAV | SF_FORMAT_PCM_16);
        if (!srcHandle) {
            std::cout << "Failed to open WAV file:" << sf_strerror(nullptr) << std::endl;
            return {};
        }

        SF_VIO sf_vio;
        SndfileHandle outBuf(sf_vio.vio, &sf_vio.data, SFM_WRITE, SF_FORMAT_WAV | SF_FORMAT_PCM_16, tar_channel,
                             tar_samplerate);
        if (!outBuf) {
            std::cout << "Failed to open output file:" << sf_strerror(nullptr) << std::endl;
            return {};
        }

        r8b::CDSPResampler16 resampler(srcHandle.samplerate(), tar_samplerate, srcHandle.samplerate());

        double *op0;
        std::vector<double> tmp(srcHandle.samplerate() * srcHandle.channels());
        double total = 0;

        while (true) {
            const auto bytesRead = srcHandle.read(tmp.data(), static_cast<sf_count_t>(tmp.size()));
            if (bytesRead <= 0) {
                break;
            }

            std::vector<double> inputBuf(tmp.size() / srcHandle.channels());
            for (int i = 0; i < tmp.size(); i += srcHandle.channels()) {
                inputBuf[i / srcHandle.channels()] = tmp[i];
            }

            const int outSamples =
                resampler.process(inputBuf.data(), static_cast<int>(bytesRead) / srcHandle.channels(), op0);

            const auto bytesWritten = static_cast<double>(outBuf.write(op0, outSamples));

            if (bytesWritten != outSamples) {
                std::cout << "Error writing to output file" << std::endl;
                break;
            }
            total += bytesWritten;
        }

        if (const int endSize = static_cast<int>(static_cast<double>(srcHandle.frames()) /
                                                     static_cast<double>(srcHandle.samplerate()) * tar_samplerate -
                                                 total)) {
            std::vector<double> inputBuf(tmp.size() / srcHandle.channels());
            resampler.process(inputBuf.data(), srcHandle.samplerate(), op0);
            outBuf.write(op0, endSize);
        }

        return sf_vio;
    }
} // namespace AudioUtil
