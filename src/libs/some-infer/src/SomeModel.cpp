#include <array>
#include <cpu_provider_factory.h>
#include <dml_provider_factory.h>
#include <iostream>
#include <some-infer/SomeModel.h>

namespace Some
{
    SomeModel::SomeModel(const std::filesystem::path &modelPath, ExecutionProvider provider, const int device_id) :
        m_env(Ort::Env(ORT_LOGGING_LEVEL_WARNING, "RmvpeModel")), m_session_options(Ort::SessionOptions()),
        m_session(nullptr), m_waveform_input_name("waveform"), m_note_midi_output_name("note_midi"),
        m_note_rest_output_name("note_rest"), m_note_dur_output_name("note_dur") {
        m_session_options.DisableMemPattern();
        m_session_options.SetExecutionMode(ORT_SEQUENTIAL);
        m_session_options.SetInterOpNumThreads(4);

        // Choose execution provider based on the provided option
        if (provider == ExecutionProvider::DML) {
            Ort::Status status(OrtSessionOptionsAppendExecutionProvider_DML(m_session_options, device_id));
            if (!status.IsOK()) {
                std::cout << "Failed to enable DirectML: " << status.GetErrorMessage() << ". Falling back to CPU." << std::endl;
            } else {
                std::cout << "Use Dml execution provider" << std::endl;
            }
        }

        try {
#ifdef _WIN32
            m_session = Ort::Session(m_env, modelPath.wstring().c_str(), m_session_options);
#else
            m_session = Ort::Session(m_env, model_path.c_str(), m_session_options);
#endif
        } catch (const Ort::Exception &e) {
            std::cout << "Failed to create session: " << e.what() << std::endl;
        }
    }

    // Destructor: Release ONNX session
    SomeModel::~SomeModel() = default;

    void SomeModel::terminate() { run_options.SetTerminate(); }

    bool SomeModel::is_open() const {
        return m_session != nullptr;
    }

    // Forward pass through the model: takes waveform and threshold as inputs, returns f0 and uv as outputs
    bool SomeModel::forward(const std::vector<float> &waveform_data, std::vector<float> &note_midi,
                            std::vector<bool> &note_rest, std::vector<float> &note_dur, std::string &msg) {
        if (!m_session) {
            msg = "Session is not initialized.";
            return false;
        }
        try {
            size_t n_samples = waveform_data.size();

            std::array<int64_t, 2> input_waveform_shape = {1, static_cast<int64_t>(n_samples)};
            Ort::Value waveform_tensor = Ort::Value::CreateTensor<float>(
                m_memory_info, const_cast<float *>(waveform_data.data()), waveform_data.size(),
                input_waveform_shape.data(), input_waveform_shape.size());

            std::vector<float> midi_data;
            std::vector<int> rest_data;
            std::vector<int> dur_data;

            const char *input_names[] = {"waveform"};
            const char *output_names[] = {"note_midi", "note_rest", "note_dur"};

            const Ort::Value input_tensors[] = {std::move(waveform_tensor)};

            run_options.UnsetTerminate();
            auto output_tensors = m_session.Run(run_options, input_names, input_tensors, 1, output_names, 3);

            const float *midi_array = output_tensors.at(0).GetTensorMutableData<float>();
            note_midi.assign(midi_array,
                             midi_array + output_tensors.at(0).GetTensorTypeAndShapeInfo().GetElementCount());

            const bool *uv_array = output_tensors.at(1).GetTensorMutableData<bool>();
            note_rest.assign(uv_array, uv_array + output_tensors.at(1).GetTensorTypeAndShapeInfo().GetElementCount());

            const float *dur_array = output_tensors.at(2).GetTensorMutableData<float>();
            note_dur.assign(dur_array, dur_array + output_tensors.at(2).GetTensorTypeAndShapeInfo().GetElementCount());

            return true;
        }
        catch (const Ort::Exception &e) {
            msg = "Error during model inference: " + std::string(e.what());
            return false;
        }
    }

} // namespace Some
