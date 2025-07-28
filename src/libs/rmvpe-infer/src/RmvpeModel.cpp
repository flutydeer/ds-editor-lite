#include <rmvpe-infer/RmvpeModel.h>

#include <stdcorelib/str.h>

#include <dsinfer/Inference/InferenceDriver.h>
#include <dsinfer/Api/Drivers/Onnx/OnnxDriverApi.h>
#include <dsinfer/Api/Singers/DiffSinger/1/DiffSingerApiL1.h>

namespace Rmvpe
{
    static inline srt::Expected<srt::NO<ds::InferenceDriver>> getInferenceDriver(const srt::SynthUnit *su) {
        if (!su) {
            return srt::Error(srt::Error::SessionError, "SynthUnit is nullptr");
        }
        auto inferenceCate = su->category("inference");
        auto dsdriverObject = inferenceCate->getFirstObject("dsdriver");

        if (!dsdriverObject) {
            return srt::Error(srt::Error::SessionError, "could not find dsdriver");
        }

        auto onnxDriver = dsdriverObject.as<ds::InferenceDriver>();

        const auto arch = onnxDriver->arch();
        constexpr auto expectedArch = ds::Api::DiffSinger::L1::API_NAME;
        const bool isArchMatch = arch == expectedArch;

        const auto backend = onnxDriver->backend();
        constexpr auto expectedBackend = ds::Api::Onnx::API_NAME;
        const bool isBackendMatch = backend == expectedBackend;

        if (!isArchMatch || !isBackendMatch) {
            return srt::Error(
                srt::Error::SessionError,
                stdc::formatN(
                    R"(invalid driver: expected arch "%1", got "%2" (%3); expected backend "%4", got "%5" (%6))",
                    expectedArch, arch, (isArchMatch ? "match" : "MISMATCH"), expectedBackend,
                    backend, (isBackendMatch ? "match" : "MISMATCH")));
        }

        return onnxDriver;
    }

    RmvpeModel::RmvpeModel(const srt::SynthUnit *su) :
        m_su(su) {
    }

    RmvpeModel::~RmvpeModel() = default;

    srt::Expected<void> RmvpeModel::open(const std::filesystem::path &modelPath) {
        if (!m_driver) {
            if (auto exp = getInferenceDriver(m_su); !exp) {
                return exp.takeError();
            } else {
                m_driver = exp.take();
            }
        }
        auto session = m_driver->createSession();
        if (!session) {
            return srt::Error(srt::Error::SessionError, "could not create session");
        }
        if (auto exp = session->open(modelPath, srt::NO<ds::Api::Onnx::SessionOpenArgs>::create()); !exp) {
            return exp.takeError();
        }
        m_session = std::move(session);
        return srt::Expected<void>();
    }

    void RmvpeModel::close() {
        if (m_session) {
            m_session->stop();
            m_session->close();
            m_session.reset();
        }
    }

    void RmvpeModel::terminate() {
        if (m_session) {
            m_session->stop();
        }
    }

    bool RmvpeModel::is_open() const {
        return m_session != nullptr;
    }

    template <typename T>
    static srt::Expected<std::vector<T>> extractTensor(const std::map<std::string, srt::NO<ds::ITensor>> &outputs,
                                                       const std::string &name) {

        const auto it = outputs.find(name);
        if (it == outputs.end()) {
            return srt::Error(srt::Error::SessionError, "missing output: " + name);
        }
        const auto &tensor = it->second;
        if (tensor->dataType() != ds::tensor_traits<T>::data_type) {
            return srt::Error(srt::Error::SessionError, "data type mismatch: " + name);
        }
        const auto data = tensor->view<T>();
        if (data.empty()) {
            return srt::Error(srt::Error::SessionError, "could not get output data: " + name);
        }
        return data.vec();
    }

    template <>
    srt::Expected<std::vector<bool>> extractTensor(const std::map<std::string, srt::NO<ds::ITensor>> &outputs,
                                                   const std::string &name) {

        const auto it = outputs.find(name);
        if (it == outputs.end()) {
            return srt::Error(srt::Error::SessionError, "missing output: " + name);
        }
        const auto &tensor = it->second;
        if (tensor->dataType() != ds::tensor_traits<bool>::data_type) {
            return srt::Error(srt::Error::SessionError, "data type mismatch: " + name);
        }
        const auto data = tensor->rawView();
        if (data.empty()) {
            return srt::Error(srt::Error::SessionError, "could not get output data: " + name);
        }
        std::vector<bool> output(data.size(), false);
        for (size_t i = 0; i < data.size(); ++i) {
            output[i] = data[i] != std::byte{0};
        }
        return output;
    }

    // Forward pass through the model: takes waveform and threshold as inputs, returns f0 and uv as outputs
    srt::Expected<void> RmvpeModel::forward(const std::vector<float> &waveform_data, float threshold,
                                            std::vector<float> &f0, std::vector<bool> &uv) {
        if (!m_session) {
            return srt::Error(srt::Error::SessionError, "RMVPE session is not initialized.");
        }

        const size_t n_samples = waveform_data.size();
        const std::vector<int64_t> input_waveform_shape = {1, static_cast<int64_t>(n_samples)};
        auto sessionInput = srt::NO<ds::Api::Onnx::SessionStartInput>::create();

        if (auto exp = ds::Tensor::createFromView<float>(input_waveform_shape, waveform_data); !exp) {
            return exp.takeError();
        } else {
            sessionInput->inputs["waveform"] = exp.take();
        }

        if (auto exp = ds::Tensor::createScalar<float>(threshold); !exp) {
            return exp.takeError();
        } else {
            sessionInput->inputs["threshold"] = exp.take();
        }

        sessionInput->outputs = {"f0", "uv"};

        if (auto exp = m_session->start(sessionInput); !exp) {
            return exp.takeError();
        }
        const auto result = m_session->result().as<ds::Api::Onnx::SessionResult>();
        if (!result) {
            return srt::Error(srt::Error::SessionError, "could not get RMVPE session result");
        }

        if (auto exp = extractTensor<float>(result->outputs, "f0"); exp) {
            f0 = exp.take();
        } else {
            return exp.takeError();
        }

        if (auto exp = extractTensor<bool>(result->outputs, "uv"); exp) {
            uv = exp.take();
        } else {
            return exp.takeError();
        }

        return srt::Expected<void>();
    }
} // namespace Rmvpe
