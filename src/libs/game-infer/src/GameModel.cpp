#include <game-infer/GameModel.h>

#include <cmath>
#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

#include <dsinfer/Api/Drivers/Onnx/OnnxDriverApi.h>
#include <dsinfer/Api/Singers/DiffSinger/1/DiffSingerApiL1.h>
#include <dsinfer/Inference/InferenceDriver.h>
#include <dsinfer/Inference/InferenceSession.h>
#include <stdcorelib/str.h>
#include <synthrt/Core/NamedObject.h>
#include <synthrt/Support/Expected.h>

namespace Game
{
    static srt::Expected<srt::NO<ds::Tensor>> createFromBoolVector(const std::vector<int64_t> &shape,
                                                                   const std::vector<bool> &boolVec) {
        // 验证形状是否与数据大小匹配
        size_t totalElements = 1;
        for (const int64_t dim : shape) {
            totalElements *= dim;
        }

        if (totalElements != boolVec.size()) {
            std::cerr << "Error: Shape doesn't match data size!" << std::endl;
            return nullptr;
        }

        // 将std::vector<bool>转换为字节容器
        ds::Tensor::Container dataContainer;
        dataContainer.reserve(boolVec.size());
        for (const bool b : boolVec) {
            dataContainer.push_back(static_cast<std::byte>(b ? 1 : 0));
        }

        auto exp = ds::Tensor::createFromRawData(ds::ITensor::Bool, {1, static_cast<long long>(dataContainer.size())},
                                                 std::move(dataContainer));
        return exp;
    }

    static srt::Expected<srt::NO<ds::InferenceDriver>> getInferenceDriver(const srt::SynthUnit *su) {
        if (!su) {
            return srt::Error(srt::Error::SessionError, "SynthUnit is nullptr");
        }
        const auto inferenceCate = su->category("inference");
        const auto dsdriverObject = inferenceCate->getFirstObject("dsdriver");

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
                    expectedArch, arch, isArchMatch ? "match" : "MISMATCH", expectedBackend, backend,
                    isBackendMatch ? "match" : "MISMATCH"));
        }

        return onnxDriver;
    }

    // Template function to extract tensor data
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
        std::vector output(data.size(), false);
        for (size_t i = 0; i < data.size(); ++i) {
            output[i] = data[i] != std::byte{0};
        }
        return output;
    }

    GameModel::GameModel(const srt::SynthUnit *su) :
        m_su(su), m_driver(nullptr), m_encoderSession(nullptr), m_segmenterSession(nullptr),
        m_estimatorSession(nullptr), m_dur2bdSession(nullptr) {}

    GameModel::~GameModel() {
        if (m_encoderSession) {
            m_encoderSession->stop();
            m_encoderSession->close();
        }
        if (m_segmenterSession) {
            m_segmenterSession->stop();
            m_segmenterSession->close();
        }
        if (m_estimatorSession) {
            m_estimatorSession->stop();
            m_estimatorSession->close();
        }
        if (m_dur2bdSession) {
            m_dur2bdSession->stop();
            m_dur2bdSession->close();
        }
    }

    int GameModel::get_target_sample_rate() const { return m_target_sample_rate; }

    srt::Expected<void> GameModel::open(const std::filesystem::path &modelPath) {
        modelDir = modelPath;

        std::ifstream configFile(modelDir / "config.json");
        if (!configFile.is_open()) {
            throw std::runtime_error("Could not open config.json: " + (modelDir / "config.json").string());
        }
        nlohmann::json config;
        configFile >> config;
        configFile.close();

        // Load parameters from config.json
        timestep = config.value("timestep", 0.01f);
        sampleRate = config.value("samplerate", 44100);
        embeddingDim = config.value("embedding_dim", 256);
        m_target_sample_rate = config.value("samplerate", 44100);

        // Set initial parameter values from config or defaults
        m_timestep = timestep;
        m_d3pm_ts = generate_d3pm_ts(0.0f, 8); // Default D3PM settings

        // Load language if available
        if (config.contains("languages")) {
            // Default to 0 (unknown/universal) if no language specified
            m_language = 0;
        }

        // Initialize the inference driver
        if (auto exp = getInferenceDriver(m_su); !exp) {
            throw std::runtime_error("Failed to get inference driver: " + exp.error().message());
        } else {
            m_driver = exp.take();
        }

        // Load all model sessions
        auto loadSession = [&](const std::string &name) -> srt::Expected<srt::NO<ds::InferenceSession>>
        {
            const std::filesystem::path model_path = modelDir / name;
            auto session = m_driver->createSession();
            if (!session) {
                return srt::Error(srt::Error::SessionError, "could not create session for " + name);
            }
            if (auto exp = session->open(model_path, srt::NO<ds::Api::Onnx::SessionOpenArgs>::create()); !exp) {
                return exp.takeError();
            }
            return session;
        };

        if (auto exp = loadSession("encoder.onnx"); exp) {
            m_encoderSession = exp.take();
        } else {
            throw std::runtime_error("Failed to load encoder: " + exp.error().message());
        }

        if (auto exp = loadSession("segmenter.onnx"); exp) {
            m_segmenterSession = exp.take();
        } else {
            throw std::runtime_error("Failed to load segmenter: " + exp.error().message());
        }

        if (auto exp = loadSession("estimator.onnx"); exp) {
            m_estimatorSession = exp.take();
        } else {
            throw std::runtime_error("Failed to load estimator: " + exp.error().message());
        }

        if (auto exp = loadSession("bd2dur.onnx"); exp) {
            m_dur2bdSession = exp.take();
        } else {
            throw std::runtime_error("Failed to load bd2dur: " + exp.error().message());
        }

        // Set default values from config or keep defaults
        m_seg_threshold = config.value("seg_threshold", 0.2f);
        m_seg_radius_seconds = config.value("seg_radius_seconds", 0.02f);
        m_est_threshold = config.value("est_threshold", 0.2f);
        return srt::Expected<void>();
    }

    bool GameModel::is_open() const {
        return m_encoderSession != nullptr && m_segmenterSession != nullptr && m_estimatorSession != nullptr &&
            m_dur2bdSession != nullptr;
    }

    void GameModel::terminate() const {
        if (m_encoderSession)
            m_encoderSession->stop();
        if (m_segmenterSession)
            m_segmenterSession->stop();
        if (m_estimatorSession)
            m_estimatorSession->stop();
        if (m_dur2bdSession)
            m_dur2bdSession->stop();
    }

    bool GameModel::forward(const std::vector<float> &waveform_data, std::vector<bool> &boundaries,
                            std::vector<float> &durations, std::vector<float> &presence, std::vector<float> &scores,
                            std::string &msg) const {
        try {
            InferenceInput input;
            input.waveform = waveform_data;
            input.duration = static_cast<float>(waveform_data.size()) / static_cast<float>(sampleRate);
            input.known_durations = {};
            input.language = m_language;

            int T = static_cast<int>(std::ceil(input.duration / m_timestep));
            if (T <= 0)
                T = 1;
            input.maskT = std::vector(T, true);
            input.timestep = m_timestep;

            int seg_radius_frames = static_cast<int>(std::round(m_seg_radius_seconds / m_timestep));
            if (seg_radius_frames < 1)
                seg_radius_frames = 1;

            const InferenceOutput output =
                inferSlice(input, m_seg_threshold, seg_radius_frames, m_est_threshold, m_d3pm_ts);

            boundaries.clear();
            boundaries.reserve(output.boundaries.size());
            for (const uint8_t val : output.boundaries) {
                boundaries.push_back(val != 0);
            }

            durations = output.durations;
            presence = output.presence;
            scores = output.scores;

            return true;
        }
        catch (const std::exception &e) {
            msg = "Error during inference: " + std::string(e.what());
            return false;
        }
    }

    std::vector<float> GameModel::generate_d3pm_ts(const float t0, const int n_steps) {
        std::vector<float> ts;
        if (n_steps <= 0)
            return ts;
        const float step = (1.0f - t0) / static_cast<float>(n_steps);
        for (int i = 0; i < n_steps; ++i) {
            ts.push_back(t0 + i * step);
        }
        return ts;
    }

    // Parameter setter methods implementation
    void GameModel::set_seg_threshold(const float threshold) { m_seg_threshold = threshold; }

    void GameModel::set_seg_radius_seconds(const float radius) { m_seg_radius_seconds = radius; }
    void GameModel::set_seg_radius_frames(const float radiusFrames) { m_est_threshold = radiusFrames * m_timestep; }

    void GameModel::set_est_threshold(const float threshold) { m_est_threshold = threshold; }

    void GameModel::set_d3pm_ts(const std::vector<float> &ts) { m_d3pm_ts = ts; }

    void GameModel::set_language(const int language) { m_language = language; }

    std::tuple<srt::NO<ds::ITensor>, srt::NO<ds::ITensor>, srt::NO<ds::ITensor>>
    GameModel::runEncoder(const std::vector<float> &waveform, const float duration) const {
        if (waveform.empty() || !m_encoderSession) {
            return std::make_tuple(srt::NO<ds::ITensor>(nullptr), srt::NO<ds::ITensor>(nullptr),
                                   srt::NO<ds::ITensor>(nullptr));
        }

        const std::vector waveformShape = {1, static_cast<int64_t>(waveform.size())};
        const std::vector<int64_t> durationShape = {1};
        const std::vector<int64_t> languageShape = {1};

        const auto sessionInput = srt::NO<ds::Api::Onnx::SessionStartInput>::create();

        if (auto exp = ds::Tensor::createFromView<float>(waveformShape, waveform); !exp) {
            throw std::runtime_error("Failed to create waveform tensor: " + exp.error().message());
        } else {
            sessionInput->inputs["waveform"] = exp.take();
        }

        const std::vector durationVec = {duration};
        if (auto exp = ds::Tensor::createFromView<float>(durationShape, durationVec); !exp) {
            throw std::runtime_error("Failed to create duration tensor: " + exp.error().message());
        } else {
            sessionInput->inputs["duration"] = exp.take();
        }

        sessionInput->outputs = {"x_seg", "x_est", "maskT"};

        if (auto exp = m_encoderSession->start(sessionInput); !exp) {
            throw std::runtime_error("Failed to run encoder: " + exp.error().message());
        }
        const auto result = m_encoderSession->result().as<ds::Api::Onnx::SessionResult>();
        if (!result) {
            throw std::runtime_error("Could not get encoder session result");
        }

        const auto xSegIt = result->outputs.find("x_seg");
        const auto xEstIt = result->outputs.find("x_est");
        const auto maskTIt = result->outputs.find("maskT");

        srt::NO<ds::ITensor> xSeg = xSegIt != result->outputs.end() ? xSegIt->second : nullptr;
        srt::NO<ds::ITensor> xEst = xEstIt != result->outputs.end() ? xEstIt->second : nullptr;
        srt::NO<ds::ITensor> maskT = maskTIt != result->outputs.end() ? maskTIt->second : nullptr;

        return std::make_tuple(xSeg, xEst, maskT);
    }

    std::vector<uint8_t> GameModel::runDur2bd(const std::vector<float> &knownDurations,
                                              const std::vector<uint8_t> &maskT) const {
        if (knownDurations.empty() || maskT.empty() || !m_dur2bdSession) {
            return {};
        }

        const std::vector knownDurationsShape = {1, static_cast<int64_t>(knownDurations.size())};
        const std::vector maskTShape = {1, static_cast<int64_t>(maskT.size())};

        const auto sessionInput = srt::NO<ds::Api::Onnx::SessionStartInput>::create();

        if (auto exp = ds::Tensor::createFromView<float>(knownDurationsShape, knownDurations); !exp) {
            throw std::runtime_error("Failed to create known_durations tensor: " + exp.error().message());
        } else {
            sessionInput->inputs["known_durations"] = exp.take();
        }

        const std::vector<bool> maskTBool(maskT.begin(), maskT.end());
        if (auto exp = createFromBoolVector(maskTShape, maskTBool); !exp) {
            throw std::runtime_error("Failed to create maskT tensor: " + exp.error().message());
        } else {
            sessionInput->inputs["maskT"] = exp.take();
        }

        sessionInput->outputs = {"boundaries"};

        if (auto exp = m_dur2bdSession->start(sessionInput); !exp) {
            throw std::runtime_error("Failed to run dur2bd: " + exp.error().message());
        }
        const auto result = m_dur2bdSession->result().as<ds::Api::Onnx::SessionResult>();
        if (!result) {
            throw std::runtime_error("Could not get dur2bd session result");
        }

        const auto boundaryIt = result->outputs.find("boundaries");
        if (boundaryIt == result->outputs.end()) {
            throw std::runtime_error("Missing boundaries output from dur2bd");
        }

        const auto &boundaryTensor = boundaryIt->second;
        if (boundaryTensor->dataType() != ds::tensor_traits<bool>::data_type) {
            throw std::runtime_error("boundaries output has wrong data type");
        }
        const auto boundaryData = boundaryTensor->rawView();
        if (boundaryData.empty()) {
            throw std::runtime_error("Could not get boundaries data");
        }

        std::vector<uint8_t> boundaries;
        boundaries.reserve(boundaryData.size());
        for (size_t i = 0; i < boundaryData.size(); ++i) {
            boundaries.push_back(boundaryData[i] != std::byte{0} ? 1 : 0);
        }

        return boundaries;
    }

    std::vector<uint8_t> GameModel::runSegmenter(const srt::NO<ds::ITensor> &xSeg,
                                                 const std::vector<uint8_t> &knownBoundaries,
                                                 const std::vector<uint8_t> &prevBoundaries, int language,
                                                 const srt::NO<ds::ITensor> &maskT, float threshold, int radius,
                                                 const std::vector<float> &d3pmTs) const {
        if (d3pmTs.empty() || !m_segmenterSession)
            return prevBoundaries;

        // Get shape information from tensors
        if (!maskT) {
            throw std::runtime_error("maskT tensor is null");
        }
        auto maskTShape = maskT->shape();
        if (maskTShape.size() != 2 || maskTShape[0] != 1) {
            throw std::runtime_error("maskT shape unexpected");
        }
        int64_t T = maskTShape[1];
        if (T <= 0)
            return {};

        auto maskTData = maskT->rawView();
        std::vector<bool> maskTBool(T);
        for (int64_t i = 0; i < T; ++i) {
            maskTBool[i] = maskTData[i] != std::byte{0} ? 1 : 0;
        }

        auto xSegShape = xSeg->shape();
        std::vector<float> xSegData;
        if (auto exp = extractTensor<float>({{"x_seg", xSeg}}, "x_seg"); exp) {
            xSegData = exp.take();
        } else {
            throw std::runtime_error("Failed to extract xSeg tensor data: " + exp.error().message());
        }

        std::vector boundariesShape = {1, T};
        std::vector xSegShapeArr = {xSegShape[0], xSegShape[1], xSegShape[2]};

        std::vector<uint8_t> currentBoundaries = prevBoundaries;
        if (currentBoundaries.size() != static_cast<size_t>(T))
            currentBoundaries.resize(T, 0);

        std::vector<uint8_t> knownBd = knownBoundaries;
        if (knownBd.size() != static_cast<size_t>(T))
            knownBd.resize(T, 0);

        for (float t : d3pmTs) {
            auto sessionInput = srt::NO<ds::Api::Onnx::SessionStartInput>::create();

            if (auto exp = ds::Tensor::createFromView<float>(xSegShapeArr, xSegData); !exp) {
                throw std::runtime_error("Failed to create xSeg tensor: " + exp.error().message());
            } else {
                sessionInput->inputs["x_seg"] = exp.take();
            }

            if (auto exp = createFromBoolVector({1, static_cast<long long>(maskTBool.size())}, maskTBool); !exp) {
                throw std::runtime_error("Failed to create maskT tensor: " + exp.error().message());
            } else {
                sessionInput->inputs["maskT"] = exp.take();
            }

            std::vector<bool> knownBdVec(knownBd.begin(), knownBd.end());
            if (auto exp = createFromBoolVector({1, static_cast<long long>(knownBdVec.size())}, knownBdVec); !exp) {
                throw std::runtime_error("Failed to create known_boundaries tensor: " + exp.error().message());
            } else {
                sessionInput->inputs["known_boundaries"] = exp.take();
            }

            std::vector<bool> currentBdVec(currentBoundaries.begin(), currentBoundaries.end());
            if (auto exp = createFromBoolVector({1, static_cast<long long>(currentBdVec.size())}, currentBdVec); !exp) {
                throw std::runtime_error("Failed to create prev_boundaries tensor: " + exp.error().message());
            } else {
                sessionInput->inputs["prev_boundaries"] = exp.take();
            }

            std::vector langVec = {static_cast<int64_t>(language)};
            if (auto exp = ds::Tensor::createFromView<int64_t>({1}, langVec); !exp) {
                throw std::runtime_error("Failed to create language tensor: " + exp.error().message());
            } else {
                sessionInput->inputs["language"] = exp.take();
            }

            std::vector threshVec = {threshold};
            if (auto exp = ds::Tensor::createFromView<float>({1}, threshVec); !exp) {
                throw std::runtime_error("Failed to create threshold tensor: " + exp.error().message());
            } else {
                sessionInput->inputs["threshold"] = exp.take();
            }

            std::vector radiusVec = {static_cast<int64_t>(radius)};
            if (auto exp = ds::Tensor::createFromView<int64_t>({1}, radiusVec); !exp) {
                throw std::runtime_error("Failed to create radius tensor: " + exp.error().message());
            } else {
                sessionInput->inputs["radius"] = exp.take();
            }

            std::vector tVec = {t};
            if (auto exp = ds::Tensor::createFromView<float>({1}, tVec); !exp) {
                throw std::runtime_error("Failed to create t tensor: " + exp.error().message());
            } else {
                sessionInput->inputs["t"] = exp.take();
            }

            sessionInput->outputs = {"boundaries"};

            if (auto exp = m_segmenterSession->start(sessionInput); !exp) {
                throw std::runtime_error("Failed to run segmenter: " + exp.error().message());
            }
            const auto result = m_segmenterSession->result().as<ds::Api::Onnx::SessionResult>();
            if (!result) {
                throw std::runtime_error("Could not get segmenter session result");
            }

            auto boundaryIt = result->outputs.find("boundaries");
            if (boundaryIt == result->outputs.end()) {
                throw std::runtime_error("Missing boundaries output from segmenter");
            }

            const auto &boundaryTensor = boundaryIt->second;
            if (boundaryTensor->dataType() != ds::tensor_traits<bool>::data_type) {
                throw std::runtime_error("boundaries output has wrong data type");
            }
            const auto boundaryData = boundaryTensor->rawView();
            if (boundaryData.empty()) {
                throw std::runtime_error("Could not get boundaries data");
            }

            currentBoundaries.clear();
            currentBoundaries.reserve(boundaryData.size());
            for (size_t i = 0; i < boundaryData.size(); ++i) {
                currentBoundaries.push_back(boundaryData[i] != std::byte{0} ? 1 : 0);
            }
            if (currentBoundaries.size() != static_cast<size_t>(T))
                currentBoundaries.resize(T, 0);
        }
        return currentBoundaries;
    }

    std::vector<uint8_t> GameModel::runSegmenterWithConfig(const srt::NO<ds::ITensor> &xSeg,
                                                           const std::vector<uint8_t> &knownBoundaries,
                                                           const std::vector<uint8_t> &prevBoundaries,
                                                           const int language, const srt::NO<ds::ITensor> &maskT,
                                                           const float threshold, const int radius,
                                                           const std::vector<float> &d3pmTs) const {
        return runSegmenter(xSeg, knownBoundaries, prevBoundaries, language, maskT, threshold, radius, d3pmTs);
    }

    std::tuple<std::vector<float>, std::vector<uint8_t>> GameModel::runBd2dur(const std::vector<uint8_t> &boundaries,
                                                                              const std::vector<uint8_t> &maskT) const {
        if (boundaries.empty() || maskT.empty() || !m_dur2bdSession) {
            return {{}, {}};
        }

        const std::vector boundariesShape = {1, static_cast<int64_t>(boundaries.size())};
        const std::vector maskTShape = {1, static_cast<int64_t>(maskT.size())};

        const auto sessionInput = srt::NO<ds::Api::Onnx::SessionStartInput>::create();

        const std::vector<bool> boundariesVec(boundaries.begin(), boundaries.end());
        if (auto exp = createFromBoolVector(boundariesShape, boundariesVec); !exp) {
            throw std::runtime_error("Failed to create boundaries tensor: " + exp.error().message());
        } else {
            sessionInput->inputs["boundaries"] = exp.take();
        }

        const std::vector<bool> maskTVec(maskT.begin(), maskT.end());
        if (auto exp = createFromBoolVector(maskTShape, maskTVec); !exp) {
            throw std::runtime_error("Failed to create maskT tensor: " + exp.error().message());
        } else {
            sessionInput->inputs["maskT"] = exp.take();
        }

        sessionInput->outputs = {"durations", "maskN"};

        if (auto exp = m_dur2bdSession->start(sessionInput); !exp) {
            throw std::runtime_error("Failed to run bd2dur: " + exp.error().message());
        }
        const auto result = m_dur2bdSession->result().as<ds::Api::Onnx::SessionResult>();
        if (!result) {
            throw std::runtime_error("Could not get bd2dur session result");
        }

        // Extract durations
        auto durIt = result->outputs.find("durations");
        if (durIt == result->outputs.end()) {
            throw std::runtime_error("Missing durations output from bd2dur");
        }
        const auto &durTensor = durIt->second;
        if (durTensor->dataType() != ds::tensor_traits<float>::data_type) {
            throw std::runtime_error("durations output has wrong data type");
        }
        const auto durData = durTensor->view<float>();
        if (durData.empty()) {
            throw std::runtime_error("Could not get durations data");
        }
        std::vector<float> durations = durData.vec();

        // Extract maskN
        auto maskNIt = result->outputs.find("maskN");
        if (maskNIt == result->outputs.end()) {
            throw std::runtime_error("Missing maskN output from bd2dur");
        }
        const auto &maskNTensor = maskNIt->second;
        if (maskNTensor->dataType() != ds::tensor_traits<bool>::data_type) {
            throw std::runtime_error("maskN output has wrong data type");
        }
        const auto maskNData = maskNTensor->rawView();
        if (maskNData.empty()) {
            throw std::runtime_error("Could not get maskN data");
        }
        std::vector<uint8_t> maskN;
        maskN.reserve(maskNData.size());
        for (size_t i = 0; i < maskNData.size(); ++i) {
            maskN.push_back(maskNData[i] != std::byte{0} ? 1 : 0);
        }

        return std::make_tuple(durations, maskN);
    }

    std::tuple<std::vector<float>, std::vector<float>> GameModel::runEstimator(const srt::NO<ds::ITensor> &xEst,
                                                                               const std::vector<uint8_t> &boundaries,
                                                                               const srt::NO<ds::ITensor> &maskT,
                                                                               const std::vector<uint8_t> &maskN,
                                                                               float threshold) const {
        if (boundaries.empty() || maskN.empty() || !m_estimatorSession) {
            return {{}, {}};
        }

        if (!maskT) {
            throw std::runtime_error("runEstimator: maskT is null");
        }
        auto maskTShape = maskT->shape();
        if (maskTShape.size() != 2 || maskTShape[0] != 1) {
            throw std::runtime_error("runEstimator: maskT shape unexpected");
        }
        int64_t T = maskTShape[1];
        if (T <= 0) {
            return {{}, {}};
        }

        auto maskTData = maskT->rawView();
        std::vector<uint8_t> maskTVec;
        maskTVec.reserve(T);
        for (int64_t i = 0; i < T; ++i) {
            maskTVec.push_back(maskTData[i] != std::byte{0});
        }

        std::vector<uint8_t> boundariesAdjusted = boundaries;
        if (boundariesAdjusted.size() != static_cast<size_t>(T)) {
            boundariesAdjusted.resize(T, 0);
        }

        if (!xEst) {
            throw std::runtime_error("runEstimator: xEst is null");
        }
        auto xEstShape = xEst->shape();
        if (xEstShape.size() < 2 || xEstShape[1] != T) {
            throw std::runtime_error("runEstimator: xEst shape mismatch with T");
        }
        std::vector<float> xEstData;
        if (auto exp = extractTensor<float>({{"x_est", xEst}}, "x_est"); exp) {
            xEstData = exp.take();
        } else {
            throw std::runtime_error("Failed to extract xEst tensor data: " + exp.error().message());
        }

        auto sessionInput = srt::NO<ds::Api::Onnx::SessionStartInput>::create();

        if (auto exp = ds::Tensor::createFromView<float>(xEstShape, xEstData); !exp) {
            throw std::runtime_error("Failed to create xEst tensor: " + exp.error().message());
        } else {
            sessionInput->inputs["x_est"] = exp.take();
        }

        std::vector<bool> uint8_boundariesAdjusted(boundariesAdjusted.begin(), boundariesAdjusted.end());
        if (auto exp =
                createFromBoolVector({1, static_cast<int64_t>(boundariesAdjusted.size())}, uint8_boundariesAdjusted);
            !exp) {
            throw std::runtime_error("Failed to create boundaries tensor: " + exp.error().message());
        } else {
            sessionInput->inputs["boundaries"] = exp.take();
        }

        std::vector<bool> uint8_maskTVec(maskTVec.begin(), maskTVec.end());
        if (auto exp = createFromBoolVector({1, static_cast<int64_t>(maskTVec.size())}, uint8_maskTVec); !exp) {
            throw std::runtime_error("Failed to create maskT tensor: " + exp.error().message());
        } else {
            sessionInput->inputs["maskT"] = exp.take();
        }

        std::vector<bool> tempMaskN(maskN.begin(), maskN.end());
        if (auto exp = createFromBoolVector({1, static_cast<int64_t>(tempMaskN.size())}, tempMaskN); !exp) {
            throw std::runtime_error("Failed to create maskN tensor: " + exp.error().message());
        } else {
            sessionInput->inputs["maskN"] = exp.take();
        }

        std::vector threshVec = {threshold};
        if (auto exp = ds::Tensor::createFromView<float>({1}, threshVec); !exp) {
            throw std::runtime_error("Failed to create threshold tensor: " + exp.error().message());
        } else {
            sessionInput->inputs["threshold"] = exp.take();
        }

        sessionInput->outputs = {"presence", "scores"};

        if (auto exp = m_estimatorSession->start(sessionInput); !exp) {
            throw std::runtime_error("Failed to run estimator: " + exp.error().message());
        }
        const auto result = m_estimatorSession->result().as<ds::Api::Onnx::SessionResult>();
        if (!result) {
            throw std::runtime_error("Could not get estimator session result");
        }

        // Extract presence
        auto presIt = result->outputs.find("presence");
        if (presIt == result->outputs.end()) {
            throw std::runtime_error("Missing presence output from estimator");
        }
        const auto &presTensor = presIt->second;
        if (presTensor->dataType() != ds::tensor_traits<bool>::data_type) {
            throw std::runtime_error("presence output has wrong data type");
        }
        const auto presData = presTensor->rawView();
        if (presData.empty()) {
            throw std::runtime_error("Could not get presence data");
        }
        std::vector<float> presence;
        presence.reserve(presData.size());
        for (size_t i = 0; i < presData.size(); ++i) {
            presence.push_back(presData[i] != std::byte{0} ? 1.0f : 0.0f);
        }

        // Extract scores
        auto scoresIt = result->outputs.find("scores");
        if (scoresIt == result->outputs.end()) {
            throw std::runtime_error("Missing scores output from estimator");
        }
        const auto &scoresTensor = scoresIt->second;
        if (scoresTensor->dataType() != ds::tensor_traits<float>::data_type) {
            throw std::runtime_error("scores output has wrong data type");
        }
        const auto scoresData = scoresTensor->view<float>();
        if (scoresData.empty()) {
            throw std::runtime_error("Could not get scores data");
        }
        std::vector<float> scores = scoresData.vec();

        return std::make_tuple(presence, scores);
    }

    InferenceOutput GameModel::inferSlice(const InferenceInput &input, float segThreshold, int segRadius,
                                          float estThreshold, const std::vector<float> &d3pmTs) const {
        InferenceOutput output;

        auto [xSegVal, xEstVal, maskTVal] = runEncoder(input.waveform, input.duration);

        if (!xSegVal || !xEstVal || !maskTVal) {
            return output;
        }

        auto xSegShape = xSegVal->shape();
        auto xEstShape = xEstVal->shape();
        auto maskTShape = maskTVal->shape();

        int64_t T = maskTShape[1];
        if (T <= 0) {
            return output;
        }

        auto maskTData = maskTVal->rawView();
        std::vector<uint8_t> maskTBool(T);
        for (int64_t i = 0; i < T; ++i) {
            if (maskTData.size() > i) {
                maskTBool[i] = maskTData[i] != std::byte{0} ? 1 : 0;
            } else {
                maskTBool[i] = false;
            }
        }

        std::vector<float> knownDurations = input.known_durations;
        if (knownDurations.empty()) {
            knownDurations.push_back(input.duration);
        }
        std::vector<uint8_t> knownBoundaries;
        knownBoundaries.resize(T, 0);

        std::vector<uint8_t> boundaries = runSegmenterWithConfig(
            xSegVal, knownBoundaries, knownBoundaries, input.language, maskTVal, segThreshold, segRadius, d3pmTs);
        if (boundaries.empty()) {
            boundaries.resize(T, 0);
        }

        std::vector<float> durations;
        std::vector<uint8_t> maskN;
        std::tie(durations, maskN) = runBd2dur(boundaries, maskTBool);
        if (durations.empty() || maskN.empty()) {
            output.boundaries = boundaries;
            return output;
        }

        std::vector<float> presence, scores;
        std::tie(presence, scores) = runEstimator(xEstVal, boundaries, maskTVal, maskN, estThreshold);

        output.boundaries = boundaries;
        output.durations = durations;
        output.presence = presence;
        output.scores = scores;
        output.maskN = maskN;

        return output;
    }
} // namespace Game
