#include <audio-util/Resample.h>
#include <iostream>

#include "CDSPResampler.h"

namespace AudioUtil
{
    SF_VIO resample(const std::filesystem::path &filepath, int tar_channel, int tar_samplerate) {
        SndfileHandle srcHandle(filepath.c_str(), SFM_READ, SF_FORMAT_WAV | SF_FORMAT_PCM_16);
        if (!srcHandle) {
            std::cout << "Failed to open WAV file:" << sf_strerror(nullptr) << std::endl;
            return {};
        }

        // 临时文件
        SF_VIO sf_vio;
        SndfileHandle outBuf(sf_vio.vio, &sf_vio.data, SFM_WRITE, SF_FORMAT_WAV | SF_FORMAT_PCM_16, tar_channel,
                             tar_samplerate);
        if (!outBuf) {
            std::cout << "Failed to open output file:" << sf_strerror(nullptr) << std::endl;
            return {};
        }

        // 创建 CDSPResampler 对象
        r8b::CDSPResampler16 resampler(srcHandle.samplerate(), tar_samplerate, srcHandle.samplerate());

        // 重采样并写入输出文件
        double *op0;
        std::vector<double> tmp(srcHandle.samplerate() * srcHandle.channels());
        double total = 0;

        // 逐块读取、重采样并写入输出文件
        while (true) {
            const auto bytesRead = srcHandle.read(tmp.data(), static_cast<sf_count_t>(tmp.size()));
            if (bytesRead <= 0) {
                break; // 读取结束
            }

            // 转单声道
            std::vector<double> inputBuf(tmp.size() / srcHandle.channels());
            for (int i = 0; i < tmp.size(); i += srcHandle.channels()) {
                inputBuf[i / srcHandle.channels()] = tmp[i];
            }

            // 处理重采样
            const int outSamples =
                resampler.process(inputBuf.data(), static_cast<int>(bytesRead) / srcHandle.channels(), op0);

            // 写入输出文件
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
