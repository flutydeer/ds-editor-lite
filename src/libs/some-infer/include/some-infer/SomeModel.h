#ifndef SOMEMODEL_H
#define SOMEMODEL_H

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

namespace Some
{
    class SomeModel {
    public:
        explicit SomeModel(const srt::SynthUnit *su);
        ~SomeModel();

        srt::Expected<void> open(const std::filesystem::path &modelPath);
        void close();
        bool is_open() const;
        // Forward pass through the model
        srt::Expected<void> forward(const std::vector<float> &waveform_data, std::vector<float> &note_midi,
                                    std::vector<bool> &note_rest, std::vector<float> &note_dur);

        void terminate();

    private:
        const srt::SynthUnit *const m_su = nullptr;
        srt::NO<ds::InferenceDriver> m_driver;
        srt::NO<ds::InferenceSession> m_session;
    };

} // namespace Some

#endif // SOMEMODEL_H
