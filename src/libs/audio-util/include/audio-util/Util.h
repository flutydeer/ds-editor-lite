#pragma once

#include <filesystem>

#include <audio-util/AudioUtilGlobal.h>
#include <audio-util/SndfileVio.h>

namespace AudioUtil
{
    bool AUDIO_UTIL_EXPORT write_audio_to_vio(const std::filesystem::path &filepath, SF_VIO &sf_vio, std::string &msg);
    bool AUDIO_UTIL_EXPORT write_vio_to_wav(SF_VIO &sf_vio_in, const std::filesystem::path &filepath,
                                              int tar_channel = -1);
    SF_VIO AUDIO_UTIL_EXPORT resample(SF_VIO &sf_vio_in, int tar_channel, int tar_samplerate);
} // namespace AudioUtil
