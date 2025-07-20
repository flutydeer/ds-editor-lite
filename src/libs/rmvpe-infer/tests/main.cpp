#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <string>
#include <vector>

#include <stdcorelib/str.h>
#include <stdcorelib/system.h>
#include <synthrt/Core/Contribute.h>
#include <synthrt/Core/NamedObject.h>
#include <synthrt/Core/SynthUnit.h>
#include <dsinfer/Inference/InferenceDriverPlugin.h>
#include <dsinfer/Api/Drivers/Onnx/OnnxDriverApi.h>

#include <rmvpe-infer/Rmvpe.h>

using EP = ds::Api::Onnx::ExecutionProvider;

static void progressChanged(const int progress) { std::cout << "progress: " << progress << "%" << std::endl; }

static srt::Expected<void> initializeSU(srt::SynthUnit &su, EP ep, int deviceIndex) {
    // Get basic directories
    auto appDir = stdc::system::application_directory();
    auto defaultPluginDir =
        appDir.parent_path() / _TSTR("lib") / _TSTR("plugins") / _TSTR("dsinfer");

    // Set default plugin directories
    su.addPluginPath("org.openvpi.InferenceDriver", defaultPluginDir / _TSTR("inferencedrivers"));

    // Load driver
    auto plugin = su.plugin<ds::InferenceDriverPlugin>("onnx");
    if (!plugin) {
        return srt::Error(srt::Error::FileNotOpen,"failed to load inference driver");
    }

    auto onnxDriver = plugin->create();
    auto onnxArgs = srt::NO<ds::Api::Onnx::DriverInitArgs>::create();

    onnxArgs->ep = ep;
    onnxArgs->runtimePath = plugin->path().parent_path() / _TSTR("runtimes");
    onnxArgs->deviceIndex = deviceIndex;

    if (auto exp = onnxDriver->initialize(onnxArgs); !exp) {
        return srt::Error(srt::Error::FileNotOpen,
            stdc::formatN(R"(failed to initialize onnx driver: %1)", exp.error().message()));
    }

    // Add driver
    auto &ic = *su.category("inference");
    ic.addObject("dsdriver", onnxDriver);
    return srt::Expected<void>();
}

static void writeCsv(const std::string &csvFilename, const std::vector<float> &f0, const std::vector<bool> &uv) {
    std::ofstream csvFile(csvFilename);
    if (!csvFile.is_open()) {
        std::cerr << "Error opening " << csvFilename << " for writing!" << std::endl;
        return;
    }

    int timeMs = 0;
    for (size_t i = 0; i < f0.size(); ++i) {
        csvFile << std::fixed << std::setprecision(5) << static_cast<float>(timeMs) / 1000.0f << "," << f0[i] << ","
                << (uv[i] ? 1 : 0) << "\n";
        timeMs += 10;
    }

    csvFile.close();
    std::cout << "CSV file '" << csvFilename << "' created successfully." << std::endl;
}

void runInference(Rmvpe::Rmvpe &rmvpe, const std::filesystem::path &wavPath, const float threshold,
                  std::vector<Rmvpe::RmvpeRes> &res, std::string &msg,
                  const std::function<void(int)> &progressChanged) {
    const bool success = rmvpe.get_f0(wavPath, threshold, res, msg, progressChanged);

    if (!success) {
        std::cerr << "Error: " << msg << std::endl;
    }
}

int main(const int argc, char *argv[]) {
    if (argc != 5 && argc != 6) {
        std::cerr << "Usage: " << argv[0] << " <model_path> <wav_path> <dml/cpu> <device_id> [csv_output]" << std::endl;
        return 1;
    }

    const std::filesystem::path modelPath = argv[1];
    const std::filesystem::path wavPath = argv[2];
    const std::string provider = argv[3];
    const int device_id = std::stoi(argv[4]);
    const std::string csvOutput = argc == 6 ? argv[5] : "";

    const auto rmProvider = [](const std::string &provider_) -> EP {
        auto provider_lower = stdc::to_lower(provider_);
        if (provider_lower == "dml" || provider_lower == "directml") {
            return EP::DMLExecutionProvider;
        }
        if (provider_lower == "cuda") {
            return EP::CUDAExecutionProvider;
        }
        if (provider_lower == "coreml") {
            return EP::CoreMLExecutionProvider;
        }
        return EP::CPUExecutionProvider;
    }(provider);

    srt::SynthUnit su;
    if (auto exp = initializeSU(su, rmProvider, device_id); !exp) {
        std::cerr << "failed to initialize SynthUnit: " << exp.error().message() << std::endl;
        return 1;
    }

    Rmvpe::Rmvpe rmvpe(&su);
    if (auto exp = rmvpe.open(modelPath); !exp) {
        std::cerr << "failed to open RMVPE model " << modelPath << ": " << exp.error().message() << std::endl;
        return 1;
    }
    constexpr float threshold = 0.03f;

    std::vector<Rmvpe::RmvpeRes> res;
    std::string msg;

    auto inferenceTask = [&rmvpe, &wavPath, &threshold, &res, &msg]
    { runInference(rmvpe, wavPath, threshold, res, msg, progressChanged); };

    std::future<void> inferenceFuture = std::async(std::launch::async, inferenceTask);
    //
    // std::thread terminateThread(terminateRmvpeAfterDelay, std::ref(rmvpe), 10);
    // terminateThread.join();

    inferenceFuture.get();

    // if (!f0.empty()) {
    //     // std::cout << "midi output:" << std::endl;
    //     // const auto midi = freqToMidi(f0);
    //     // for (const float value : midi) {
    //     //     std::cout << value << " ";
    //     // }
    //     // std::cout << std::endl;
    //
    //     if (!csvOutput.empty() && csvOutput.substr(csvOutput.find_last_of('.')) == ".csv") {
    //         writeCsv(csvOutput, f0, uv);
    //     } else if (!csvOutput.empty()) {
    //         std::cerr << "Error: The CSV output path must end with '.csv'." << std::endl;
    //     }
    // } else {
    //     std::cerr << "Error: " << msg << std::endl;
    // }

    return 0;
}
