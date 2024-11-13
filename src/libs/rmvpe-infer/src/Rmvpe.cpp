#include <rmvpe-infer/Rmvpe.h>

#include <sndfile.hh>

#include <iostream>

#include <audio-util/Resample.h>
#include <audio-util/Slicer.h>
#include <rmvpe-infer/RmvpeModel.h>

namespace Rmvpe
{
    Rmvpe::Rmvpe(const std::filesystem::path &modelPath, ExecutionProvider provider, int device_id) {
        m_rmvpe = std::make_unique<RmvpeModel>(modelPath, provider, device_id);

        if (!m_rmvpe) {
            std::cout << "Cannot load ASR Model, there must be files model.onnx and vocab.txt" << std::endl;
        }
    }

    Rmvpe::~Rmvpe() = default;

    bool Rmvpe::get_f0(AudioUtil::SF_VIO sf_vio, float threshold, std::vector<float> &f0, std::vector<bool> &uv,
                       std::string &msg, const std::function<void(int)>& progressChanged) {
        if (!m_rmvpe) {
            return false;
        }
        terminated = false;
        SndfileHandle sf(sf_vio.vio, &sf_vio.data, SFM_READ, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 1, 16000);
        AudioUtil::Slicer slicer(&sf, -40, 5000, 300, 10, 1000);

        const auto chunks = slicer.slice();

        if (chunks.empty()) {
            msg = "slicer: no audio chunks for output!";
            return false;
        }

        const auto frames = sf.frames();
        const auto totalSize = frames;

        int processedFrames = 0; // To track processed frames

        for (const auto &chunk : chunks) {
            if (terminated)
                break;
            const auto beginFrame = chunk.first;
            const auto endFrame = chunk.second;
            const auto frameCount = endFrame - beginFrame;
            if (frameCount <= 0 || beginFrame > totalSize || endFrame > totalSize) {
                continue;
            }

            AudioUtil::SF_VIO sfChunk;
            auto wf = SndfileHandle(sfChunk.vio, &sfChunk.data, SFM_WRITE, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 1, 16000);
            sf.seek(static_cast<sf_count_t>(beginFrame), SEEK_SET);
            std::vector<float> tmp(frameCount);
            sf.read(tmp.data(), static_cast<sf_count_t>(tmp.size()));
            const auto bytesWritten = wf.write(tmp.data(), static_cast<sf_count_t>(tmp.size()));

            std::vector<float> temp_f0;
            std::vector<bool> temp_uv;
            const bool success = m_rmvpe->forward(tmp, threshold, temp_f0, temp_uv, msg);
            if (!success)
                return false;
            f0.insert(f0.end(), temp_f0.begin(), temp_f0.end());
            uv.insert(uv.end(), temp_uv.begin(), temp_uv.end());


            // Update the processed frames and calculate progress
            processedFrames += static_cast<int>(frameCount);
            int progress = static_cast<int>((static_cast<float>(processedFrames) / totalSize) * 100);

            // Call the progress callback with the updated progress
            if (progressChanged) {
                progressChanged(progress); // Trigger the callback with the progress value
            }
        }
        return true;
    }

    bool Rmvpe::get_f0(const std::filesystem::path &filepath, float threshold, std::vector<float> &f0,
                       std::vector<bool> &uv, std::string &msg, const std::function<void(int)> &progressChanged) {
        return get_f0(AudioUtil::resample(filepath, 1, 16000), threshold, f0, uv, msg, progressChanged);
    }

    void Rmvpe::terminate() {
        m_rmvpe->terminate();
        terminated = true;
    }

} // namespace Rmvpe
