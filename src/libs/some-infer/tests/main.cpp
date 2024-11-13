#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <some-infer/Some.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <model_path> <wav_path>" << std::endl;
        return 1;
    }

    const std::filesystem::path modelPath = argv[1];
    const std::filesystem::path wavPath = argv[2];

    const Some::Some some(modelPath, 1);

    std::vector<float> note_midi;
    std::vector<bool> note_rest;
    std::vector<float> note_dur;
    std::string msg;

    bool success = some.get_midi(wavPath, note_midi, note_rest, note_dur, msg);

    if (success) {
        std::cout << "midi output:" << std::endl;
        for (const float value : note_midi) {
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
