#include <audio-util/Util.h>

#include <iostream>
#include <soxr.h>
#include "FlacDecoder.h"
#include "Mp3Decoder.h"

namespace AudioUtil
{
    static void write_wav_to_vio(const std::filesystem::path &filepath, SF_VIO &sf_vio) {
        SF_INFO sfinfo;
        SNDFILE *infile = sf_open(filepath.string().c_str(), SFM_READ, &sfinfo);
        if (!infile) {
            throw std::runtime_error("无法打开输入文件进行读取: " + std::string(sf_strerror(nullptr)));
        }

        sf_vio.info = sfinfo;
        SndfileHandle outBuf(sf_vio.vio, &sf_vio.data, SFM_WRITE, sfinfo.format, sfinfo.channels, sfinfo.samplerate);
        if (!outBuf) {
            throw std::runtime_error("无法创建输出 VIO 句柄: " + std::string(sf_strerror(nullptr)));
        }

        constexpr int bufferSize = 1024;
        std::vector<float> buffer(bufferSize * sfinfo.channels, 0);

        while (true) {
            sf_count_t framesRead;

            if (sfinfo.format & SF_FORMAT_FLOAT) { // 32-bit float
                framesRead = sf_read_float(infile, buffer.data(), bufferSize);
            } else if (sfinfo.format & SF_FORMAT_PCM_16) { // 16-bit PCM
                std::vector<short> shortBuffer(bufferSize * sfinfo.channels);
                framesRead = sf_readf_short(infile, shortBuffer.data(), bufferSize);
                // 将 16-bit PCM 转换为 32-bit float
                std::transform(shortBuffer.begin(), shortBuffer.begin() + framesRead * sfinfo.channels, buffer.begin(),
                               [](const short sample) { return static_cast<float>(sample) / 32768.0f; });
            } else if (sfinfo.format & SF_FORMAT_PCM_24) { // 24-bit PCM
                std::vector<int> intBuffer(bufferSize * sfinfo.channels);
                framesRead = sf_readf_int(infile, intBuffer.data(), bufferSize);
                // 将 24-bit PCM 转换为 32-bit float
                std::transform(intBuffer.begin(), intBuffer.begin() + framesRead * sfinfo.channels, buffer.begin(),
                               [](const int sample) { return static_cast<float>(sample) / 8388608.0f; });
            } else {
                throw std::runtime_error("不支持的音频格式: " + std::string(sf_strerror(nullptr)));
            }

            if (framesRead == 0) {
                break;
            }

            const sf_count_t framesWritten = outBuf.writef(buffer.data(), framesRead);
            if (framesWritten < 0) {
                throw std::runtime_error("写入 VIO 失败: " + std::string(sf_strerror(nullptr)));
            }
        }
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
        sf_vio.data.seek = 44;
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
        sf_vio.info = sf_vio_in.info;
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
        std::vector<float> inputBuf(srcHandle.samplerate() * srcHandle.channels(), 0);
        std::vector<float> outputBuf(tar_samplerate * tar_channel, 0);

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

    bool write_vio_to_wav(SF_VIO &sf_vio_in, const std::filesystem::path &filepath, int tar_channel) {
        const auto [frames, samplerate, channels, format, sections, seekable] = sf_vio_in.info;

        if (tar_channel == -1)
            tar_channel = channels;

        SndfileHandle readBuf(sf_vio_in.vio, &sf_vio_in.data, SFM_READ, format, channels, samplerate);

        SndfileHandle outBuf(filepath.string(), SFM_WRITE, format, tar_channel, samplerate);

        std::vector<float> buffer(1024 * channels, 0);
        int readFrames;

        while ((readFrames = static_cast<int>(
                    readBuf.readf(buffer.data(), static_cast<sf_count_t>(buffer.size()) / channels))) > 0) {
            if (tar_channel != channels) {
                std::vector<float> convertedBuffer(1024 * tar_channel, 0);
                for (int i = 0; i < readFrames; ++i) {
                    for (int ch = 0; ch < tar_channel; ++ch) {
                        if (ch < channels) {
                            convertedBuffer[i * tar_channel + ch] = buffer[i * channels + ch];
                        } else {
                            convertedBuffer[i * tar_channel + ch] = 0;
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
