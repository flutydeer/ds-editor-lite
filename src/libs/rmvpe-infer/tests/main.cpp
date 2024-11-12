#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <rmvpe-infer/Rmvpe.h>

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

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <model_path> <wav_path>" << std::endl;
        return 1;
    }

    const std::filesystem::path modelPath = argv[1];
    std::filesystem::path wavPath = argv[2];

    const Rmvpe::Rmvpe rmvpe(modelPath);
    const float threshold = 0.03f;

    std::vector<float> f0;
    std::vector<bool> uv;
    std::string msg;

    bool success = rmvpe.get_f0(wavPath, threshold, f0, uv, msg);

    if (success) {
        std::cout << "midi output:" << std::endl;
        const auto midi = freqToMidi(f0);
        for (const float value : midi) {
            std::cout << value << " ";
        }
        std::cout << std::endl;

        // std::cout << "UV output:" << std::endl;
        // for (const bool value : uv) {
        //     std::cout << value << " ";
        // }
        // std::cout << std::endl;
    } else {
        std::cerr << "Error: " << msg << std::endl;
    }

    return 0;
}
