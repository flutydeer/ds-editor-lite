#ifndef RMVPEMODEL_H
#define RMVPEMODEL_H

#include <filesystem>
#include <string>
#include <vector>

#include <synthrt/Core/NamedObject.h>
#include <synthrt/Support/Expected.h>
#include <dsinfer/Inference/InferenceDriver.h>
#include <dsinfer/Inference/InferenceSession.h>

namespace srt
{
    class SynthUnit;
}

namespace Rmvpe
{
    class RmvpeModel {
    public:
        explicit RmvpeModel(const srt::SynthUnit *su);
        ~RmvpeModel();

        srt::Expected<void> open(const std::filesystem::path &modelPath);
        void close();

        bool is_open() const;
        // Forward pass through the model
        srt::Expected<void> forward(const std::vector<float> &waveform_data, float threshold, std::vector<float> &f0,
                     std::vector<bool> &uv);

        void terminate();

    private:
        const srt::SynthUnit *const m_su = nullptr;
        srt::NO<ds::InferenceDriver> m_driver;
        srt::NO<ds::InferenceSession> m_session;
    };

} // namespace Rmvpe

#endif // RMVPEMODEL_H
