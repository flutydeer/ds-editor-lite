#include <audio-util/Util.h>

#include <iostream>
#include <soxr.h>
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

        sf_vio.info = sfinfo;

        SndfileHandle outBuf(sf_vio.vio, &sf_vio.data, SFM_WRITE, sfinfo.format, sfinfo.channels, sfinfo.samplerate);

        short buffer[1024];
        sf_count_t frames;
        while ((frames = sf_read_short(infile, buffer, sizeof(buffer) / 2)) > 0) {
            convert_to_mono(buffer, frames, sfinfo.channels);
            outBuf.write(buffer, frames);
        }
        sf_close(infile);
    }

    bool write_audio_to_vio(const std::filesystem::path &filepath, SF_VIO &sf_vio, std::string &msg) {
        const std::string extension = filepath.extension().string();
        if (extension == ".wav") {
            write_wav_to_vio(filepath, sf_vio);
        } else if (extension == ".mp3") {
            write_mp3_to_vio(filepath, sf_vio);
        } else if (extension == ".flac") {
            write_flac_to_vio(filepath, sf_vio);
        } else {
            msg = "Unsupported file format: " + filepath.string();
            return false;
        }
        sf_vio.data.seek = 0;
        return true;
    }

    SF_VIO resample(SF_VIO &sf_vio_in, const int tar_channel, const int tar_samplerate) {
        SndfileHandle srcHandle(sf_vio_in.vio, &sf_vio_in.data, SFM_READ, sf_vio_in.info.format,
                                sf_vio_in.info.channels, sf_vio_in.info.samplerate);
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

        // Create a SoX resampler instance
        soxr_error_t error;
        const auto resampler = soxr_create(srcHandle.samplerate(), tar_samplerate, srcHandle.channels(), &error,
                                           nullptr, nullptr, nullptr);
        if (!resampler) {
            std::cout << "Failed to create SoX resampler: " << soxr_strerror(error) << std::endl;
            return {};
        }

        // Define buffers for resampling
        std::vector<float> inputBuf(srcHandle.samplerate() * srcHandle.channels());
        std::vector<float> outputBuf(tar_samplerate * tar_channel);

        size_t bytesRead;

        while ((bytesRead = srcHandle.read(inputBuf.data(), static_cast<sf_count_t>(inputBuf.size()))) > 0) {
            const size_t inputSamples = bytesRead / srcHandle.channels();
            const size_t outputSamples = outputBuf.size() / tar_channel;

            // Perform the resampling using SoX
            size_t inputDone = 0, outputDone = 0;
            const soxr_error_t err = soxr_process(resampler, inputBuf.data(), inputSamples, &inputDone,
                                                  outputBuf.data(), outputSamples, &outputDone);

            if (err != nullptr) {
                std::cout << "Error during resampling: " << soxr_strerror(err) << std::endl;
                break;
            }

            // Write the resampled data to the output file
            const size_t bytesWritten = outBuf.write(outputBuf.data(), static_cast<sf_count_t>(outputDone));
            if (bytesWritten != outputDone) {
                std::cout << "Error writing to output file" << std::endl;
                break;
            }
        }

        // Clean up
        soxr_delete(resampler);

        return sf_vio;
    }

    bool vio_to_wav(SF_VIO &sf_vio_in, const std::filesystem::path &filepath, int tar_channel, int tar_samplerate,
                    int tar_sf_format) {
        const auto [frames, samplerate, channels, format, sections, seekable] = sf_vio_in.info;

        // If no channel or sample rate is specified, use the defaults from the source
        if (tar_channel == -1)
            tar_channel = channels;

        if (tar_samplerate == -1)
            tar_samplerate = samplerate;

        if (tar_sf_format == -1)
            tar_sf_format = sf_vio_in.info.format;

        SndfileHandle readBuf(sf_vio_in.vio, &sf_vio_in.data, SFM_READ, format, channels, samplerate);

        // Create a SndfileHandle for writing
        SndfileHandle outBuf(filepath.string(), SFM_WRITE, tar_sf_format, tar_channel, tar_samplerate);

        // Buffer for reading and writing
        std::vector<short> buffer(1024 * channels); // Adjust buffer size as needed
        int readFrames;

        // Read and write loop
        while ((readFrames = static_cast<int>(
                    readBuf.readf(buffer.data(), static_cast<sf_count_t>(buffer.size()) / channels))) > 0) {
            // If the target channel is different from the source, we need to handle channel conversion
            if (tar_channel != channels) {
                std::vector<short> convertedBuffer(1024 * tar_channel);
                for (int i = 0; i < readFrames; ++i) {
                    for (int ch = 0; ch < tar_channel; ++ch) {
                        if (ch < channels) {
                            convertedBuffer[i * tar_channel + ch] = buffer[i * channels + ch];
                        } else {
                            convertedBuffer[i * tar_channel + ch] = 0; // Zero-fill for extra channels
                        }
                    }
                }
                outBuf.writef(convertedBuffer.data(), readFrames);
            } else {
                outBuf.writef(buffer.data(), readFrames);
            }
        }
        return true;
    }
} // namespace AudioUtil
