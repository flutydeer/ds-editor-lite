#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <sndfile.h>

#include <audio-util/Util.h>


int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <audio_in_path>  <wav_out_path>" << std::endl;
        return 1;
    }

    const std::filesystem::path audio_in_path = argv[1];
    const std::filesystem::path wav_out_path = argv[2];

    std::string errorMsg;
    AudioUtil::SF_VIO sf_vio_raw;
    if (!AudioUtil::write_audio_to_vio(audio_in_path, sf_vio_raw, errorMsg)) {
        return false;
    }

    // auto sf_vio = AudioUtil::resample(sf_vio_raw, 1, 16000);
    AudioUtil::write_vio_to_wav(sf_vio_raw, wav_out_path, 1);

    return 0;
}
