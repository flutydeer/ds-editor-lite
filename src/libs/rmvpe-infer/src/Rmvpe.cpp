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

        if (!is_open()) {
            std::cout << "Cannot load RMVPE Model, there must be files " + modelPath.string() << std::endl;
        }
    }

    Rmvpe::~Rmvpe() = default;

    bool Rmvpe::is_open() const { return m_rmvpe && m_rmvpe->is_open(); }

    static float linear_interpolate(const int t, const int t1, const int t2, const float v1, const float v2) {
        if (t1 == t2)
            return v1;
        return v1 + (v2 - v1) * static_cast<float>(t - t1) / static_cast<float>(t2 - t1);
    }

    static void resample_f0_uv(const std::vector<RmvpeRes> &res, std::vector<float> &target_f0,
                               std::vector<bool> &target_uv, const float totalSize) {
        constexpr int interval = 10;
        const int target_size = static_cast<int>(totalSize / interval);

        target_f0.assign(target_size, 0.0f);
        target_uv.assign(target_size, false);

        for (const auto &[offset, f0, uv] : res) {
            for (size_t i = 0; i < f0.size(); ++i) {
                const float time = offset + static_cast<float>(i * interval);
                if (time >= 0 && time <= totalSize) {
                    const int target_index = static_cast<int>(time / interval);

                    if (target_index < target_size) {
                        target_f0[target_index] = f0[i];
                        target_uv[target_index] = uv[i];
                    }
                }
            }
        }

        for (int i = 1; i < target_size; ++i) {
            if (target_f0[i] == 0.0f) {
                int prev_index = i - 1;
                int next_index = i + 1;

                while (prev_index >= 0 && target_f0[prev_index] == 0.0f)
                    prev_index--;
                while (next_index < target_size && target_f0[next_index] == 0.0f)
                    next_index++;

                if (prev_index >= 0 && next_index < target_size) {
                    const auto prev_time = prev_index * interval;
                    const auto next_time = next_index * interval;
                    target_f0[i] = linear_interpolate(i * interval, prev_time, next_time, target_f0[prev_index],
                                                      target_f0[next_index]);
                    target_uv[i] = target_uv[prev_index] && target_uv[next_index];
                }
            }
        }
    }

    static float calculateSumOfDifferences(const AudioUtil::MarkerList &markers) {
        float sum = 0;
        for (const auto &[fst, snd] : markers) {
            sum += static_cast<float>(snd - fst);
        }
        return sum;
    }

    bool Rmvpe::get_f0(AudioUtil::SF_VIO sf_vio, float threshold, std::vector<float> &f0, std::vector<bool> &uv,
                       std::string &msg, const std::function<void(int)> &progressChanged) const {
        if (!m_rmvpe) {
            return false;
        }
        std::vector<RmvpeRes> res;
        SndfileHandle sf(sf_vio.vio, &sf_vio.data, SFM_READ, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 1, 16000);
        AudioUtil::Slicer slicer(&sf, -40, 5000, 300, 10, 1000);

        const auto chunks = slicer.slice();

        if (chunks.empty()) {
            msg = "slicer: no audio chunks for output!";
            return false;
        }

        const auto totalSize = sf.frames();

        int processedFrames = 0; // To track processed frames
        const float slicerFrames = calculateSumOfDifferences(chunks);

        for (const auto &[fst, snd] : chunks) {
            const auto beginFrame = fst;
            const auto endFrame = snd;
            const auto frameCount = endFrame - beginFrame;
            if (frameCount <= 0 || beginFrame > totalSize || endFrame > totalSize) {
                continue;
            }

            AudioUtil::SF_VIO sfChunk;
            auto wf = SndfileHandle(sfChunk.vio, &sfChunk.data, SFM_WRITE, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 1, 16000);
            sf.seek(static_cast<sf_count_t>(beginFrame), SEEK_SET);
            std::vector<float> tmp(frameCount);
            sf.read(tmp.data(), static_cast<sf_count_t>(tmp.size()));
            wf.write(tmp.data(), static_cast<sf_count_t>(tmp.size()));

            RmvpeRes tempRes;
            tempRes.offset = static_cast<float>(static_cast<double>(fst) / (16000.0 / 1000));
            const bool success = m_rmvpe->forward(tmp, threshold, tempRes.f0, tempRes.uv, msg);
            if (!success)
                return false;
            res.push_back(tempRes);

            // Update the processed frames and calculate progress
            processedFrames += static_cast<int>(frameCount);
            int progress = static_cast<int>((static_cast<float>(processedFrames) / slicerFrames) * 100);

            // Call the progress callback with the updated progress
            if (progressChanged) {
                progressChanged(progress); // Trigger the callback with the progress value
            }
        }

        resample_f0_uv(res, f0, uv, static_cast<float>(static_cast<double>(totalSize) / (16000.0 / 1000)));
        return true;
    }

    bool Rmvpe::get_f0(const std::filesystem::path &filepath, const float threshold, std::vector<float> &f0,
                       std::vector<bool> &uv, std::string &msg, const std::function<void(int)> &progressChanged) const {
        return get_f0(AudioUtil::resample(filepath, 1, 16000), threshold, f0, uv, msg, progressChanged);
    }

    void Rmvpe::terminate() const { m_rmvpe->terminate(); }

} // namespace Rmvpe
