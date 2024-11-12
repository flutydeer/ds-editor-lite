#include <rmvpe-infer/Rmvpe.h>

#include <CDSPResampler.h>
#include <sndfile.hh>

#include <iostream>

#include "Slicer.h"

namespace Rmvpe
{
    Rmvpe::Rmvpe(const std::filesystem::path &modelPath) {
        m_rmvpe = std::make_unique<RmvpeModel>(modelPath);

        if (!m_rmvpe) {
            std::cout << "Cannot load ASR Model, there must be files model.onnx and vocab.txt" << std::endl;
        }
    }

    Rmvpe::~Rmvpe() = default;

    bool Rmvpe::get_f0(SF_VIO sf_vio, float threshold, std::vector<float> &f0, std::vector<bool> &uv,
                       std::string &msg) const {
        if (!m_rmvpe) {
            return false;
        }
        SndfileHandle sf(sf_vio.vio, &sf_vio.data, SFM_READ, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 1, 16000);
        Slicer slicer(&sf, -40, 5000, 300, 10, 1000);

        const auto chunks = slicer.slice();

        if (chunks.empty()) {
            msg = "slicer: no audio chunks for output!";
            return false;
        }

        const auto frames = sf.frames();
        const auto totalSize = frames;

        for (const auto &chunk : chunks) {
            const auto beginFrame = chunk.first;
            const auto endFrame = chunk.second;
            const auto frameCount = endFrame - beginFrame;
            if (frameCount <= 0 || beginFrame > totalSize || endFrame > totalSize) {
                continue;
            }

            SF_VIO sfChunk;
            auto wf = SndfileHandle(sfChunk.vio, &sfChunk.data, SFM_WRITE, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 1, 16000);
            sf.seek(static_cast<sf_count_t>(beginFrame), SEEK_SET);
            std::vector<float> tmp(frameCount);
            sf.read(tmp.data(), static_cast<sf_count_t>(tmp.size()));
            const auto bytesWritten = wf.write(tmp.data(), static_cast<sf_count_t>(tmp.size()));

            // if (bytesWritten > 60 * 16000) {
            //     msg = "The audio contains continuous pronunciation segments that exceed 60 seconds. Please manually "
            //           "segment and rerun the recognition program.";
            //     return false;
            // }

            std::vector<float> temp_f0;
            std::vector<bool> temp_uv;
            const bool success = m_rmvpe->forward(tmp, threshold, temp_f0, temp_uv, msg);
            if (!success)
                return false;
            f0.insert(f0.end(), temp_f0.begin(), temp_f0.end());
            uv.insert(uv.end(), temp_uv.begin(), temp_uv.end());
        }
        return true;
    }

    bool Rmvpe::get_f0(const std::filesystem::path &filepath, float threshold, std::vector<float> &f0,
                       std::vector<bool> &uv, std::string &msg) const {
        return get_f0(resample(filepath), threshold, f0, uv, msg);
    }

    SF_VIO Rmvpe::resample(const std::filesystem::path &filepath) {
        SndfileHandle srcHandle(filepath.c_str(), SFM_READ, SF_FORMAT_WAV | SF_FORMAT_PCM_16);
        if (!srcHandle) {
            std::cout << "Failed to open WAV file:" << sf_strerror(nullptr) << std::endl;
            return {};
        }

        // 临时文件
        SF_VIO sf_vio;
        SndfileHandle outBuf(sf_vio.vio, &sf_vio.data, SFM_WRITE, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 1, 16000);
        if (!outBuf) {
            std::cout << "Failed to open output file:" << sf_strerror(nullptr) << std::endl;
            return {};
        }

        // 创建 CDSPResampler 对象
        r8b::CDSPResampler16 resampler(srcHandle.samplerate(), 16000, srcHandle.samplerate());

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
                                                     static_cast<double>(srcHandle.samplerate()) * 16000.0 -
                                                 total)) {
            std::vector<double> inputBuf(tmp.size() / srcHandle.channels());
            resampler.process(inputBuf.data(), srcHandle.samplerate(), op0);
            outBuf.write(op0, endSize);
        }

        return sf_vio;
    }
} // Rmvpe