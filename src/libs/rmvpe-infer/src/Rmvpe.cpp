#include <rmvpe-infer/Rmvpe.h>

#include <sndfile.hh>

#include <iostream>

#include <audio-util/Slicer.h>
#include <audio-util/Util.h>
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

    static float calculateSumOfDifferences(const AudioUtil::MarkerList &markers) {
        float sum = 0;
        for (const auto &[fst, snd] : markers) {
            sum += static_cast<float>(snd - fst);
        }
        return sum;
    }

    static void interp_f0(std::vector<float> &f0, std::vector<bool> &uv) {
        const int n = static_cast<int>(f0.size());
        int first_true = -1;
        int last_true = -1;

        for (int i = 0; i < n; ++i) {
            if (!uv[i]) {
                first_true = i;
                break;
            }
        }

        for (int i = n - 1; i >= 0; --i) {
            if (!uv[i]) {
                last_true = i;
                break;
            }
        }

        if (first_true != -1) {
            for (int i = 0; i < first_true; ++i) {
                f0[i] = f0[first_true];
            }
        }

        if (last_true != -1) {
            for (int i = n - 1; i > last_true; --i) {
                f0[i] = f0[last_true];
            }
        }

        for (int i = first_true; i < last_true; ++i) {
            if (uv[i]) {
                const int prev = i - 1;
                int next = i + 1;
                while (next < n && uv[next])
                    next++;
                if (next < n) {
                    const float ratio = std::log(f0[next] / f0[prev]);
                    f0[i] = static_cast<float>(f0[prev] *
                                               std::exp(ratio * static_cast<long double>(i - prev) / (next - prev)));
                }
            }
        }
    }

    bool Rmvpe::get_f0(const std::filesystem::path &filepath, const float threshold, std::vector<RmvpeRes> &res,
                       std::string &msg, const std::function<void(int)> &progressChanged) const {
        if (!m_rmvpe) {
            return false;
        }

        auto sf_vio = AudioUtil::resample_to_vio(filepath, msg, 1, 16000);

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
            interp_f0(tempRes.f0, tempRes.uv);
            res.push_back(tempRes);

            // Update the processed frames and calculate progress
            processedFrames += static_cast<int>(frameCount);
            int progress = static_cast<int>((static_cast<float>(processedFrames) / slicerFrames) * 100);

            // Call the progress callback with the updated progress
            if (progressChanged) {
                progressChanged(progress); // Trigger the callback with the progress value
            }
        }
        return true;
    }

    void Rmvpe::terminate() const { m_rmvpe->terminate(); }

} // namespace Rmvpe
