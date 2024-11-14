#ifndef FBLMODEL_H
#define FBLMODEL_H

#include <filesystem>
#include <onnxruntime_cxx_api.h>
#include <some-infer/Provider.h>
#include <string>
#include <vector>

namespace Some
{
    class SomeModel {
    public:
        explicit SomeModel(const std::filesystem::path &modelPath, ExecutionProvider provider, int device_id);
        ~SomeModel();

        // Forward pass through the model
        bool forward(const std::vector<float> &waveform_data, std::vector<float> &note_midi,
                     std::vector<bool> &note_rest, std::vector<float> &note_dur, std::string &msg);

        void terminate();

    private:
        Ort::Env m_env;
        Ort::RunOptions run_options;
        Ort::SessionOptions m_session_options;
        std::unique_ptr<Ort::Session> m_session;
        Ort::AllocatorWithDefaultOptions m_allocator;
        const char *m_waveform_input_name; // Name of the waveform input
        const char *m_note_midi_output_name; // Name of the midi input
        const char *m_note_rest_output_name; // Name of the rest output
        const char *m_note_dur_output_name; // Name of the dur output

#ifdef _WIN_X86
        Ort::MemoryInfo m_memory_info = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
#else
        Ort::MemoryInfo m_memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
#endif
    };

} // namespace Some

#endif // FBLMODEL_H
