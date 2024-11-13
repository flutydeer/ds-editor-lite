#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <string>
#include <vector>

#include <rmvpe-infer/Rmvpe.h>

static void progressChanged(const int progress) { std::cout << "progress: " << progress << "%" << std::endl; }

static std::vector<float> freqToMidi(const std::vector<float> &frequencies) {
    std::vector<float> midiPitches;

    for (const float f : frequencies) {
        if (f > 0) {
            float midiPitch = 69 + 12 * std::log2(f / 440.0f);
            midiPitches.push_back(midiPitch);
        } else {
            midiPitches.push_back(0);
        }
    }

    return midiPitches;
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

void runInference(Rmvpe::Rmvpe &rmvpe, const std::filesystem::path &wavPath, float threshold, std::vector<float> &f0,
                  std::vector<bool> &uv, std::string &msg, const std::function<void(int)> &progressChanged) {
    bool success = rmvpe.get_f0(wavPath, threshold, f0, uv, msg, progressChanged);

    if (!success) {
        std::cerr << "Error: " << msg << std::endl;
    }
}

void terminateRmvpeAfterDelay(Rmvpe::Rmvpe &rmvpe, int delaySeconds) {
    std::this_thread::sleep_for(std::chrono::seconds(delaySeconds));
    rmvpe.terminate();
    std::cout << "Rmvpe terminated after " << delaySeconds << " seconds." << std::endl;
}

int main(int argc, char *argv[]) {
    if (argc != 5 && argc != 6) {
        std::cerr << "Usage: " << argv[0] << " <model_path> <wav_path> <dml/cpu> <device_id> [csv_output]" << std::endl;
        return 1;
    }

    const std::filesystem::path modelPath = argv[1];
    const std::filesystem::path wavPath = argv[2];
    const std::string provider = argv[3];
    const int device_id = std::stoi(argv[4]);
    const std::string csvOutput = argc == 6 ? argv[5] : "";

    const auto rmProvider = provider == "dml" ? Rmvpe::ExecutionProvider::DML : Rmvpe::ExecutionProvider::CPU;

    Rmvpe::Rmvpe rmvpe(modelPath, rmProvider, device_id);
    constexpr float threshold = 0.03f;

    std::vector<float> f0;
    std::vector<bool> uv;
    std::string msg;

    auto inferenceTask = [&rmvpe, &wavPath, &threshold, &f0, &uv, &msg]
    { runInference(rmvpe, wavPath, threshold, f0, uv, msg, progressChanged); };

    std::future<void> inferenceFuture = std::async(std::launch::async, inferenceTask);
    //
    // std::thread terminateThread(terminateRmvpeAfterDelay, std::ref(rmvpe), 10);
    // terminateThread.join();

    inferenceFuture.get();

    if (!f0.empty()) {
        std::cout << "midi output:" << std::endl;
        const auto midi = freqToMidi(f0);
        for (const float value : midi) {
            std::cout << value << " ";
        }
        std::cout << std::endl;

        if (!csvOutput.empty() && csvOutput.substr(csvOutput.find_last_of('.')) == ".csv") {
            writeCsv(csvOutput, f0, uv);
        } else if (!csvOutput.empty()) {
            std::cerr << "Error: The CSV output path must end with '.csv'." << std::endl;
        }
    } else {
        std::cerr << "Error: " << msg << std::endl;
    }

    return 0;
}
