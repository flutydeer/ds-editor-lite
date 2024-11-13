#ifndef RESAMPLE_H
#define RESAMPLE_H

#include <filesystem>

#include <audio-util/AudioUtilGlobal.h>
#include <audio-util/SndfileVio.h>

namespace AudioUtil
{
    SF_VIO AUDIO_UTIL_EXPORT resample(const std::filesystem::path &filepath, int tar_channel, int tar_samplerate);
} // namespace AudioUtil

#endif // RESAMPLE_H
