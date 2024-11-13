#ifndef SLICER_H
#define SLICER_H

#include <sndfile.hh>
#include <vector>

#include <audio-util/AudioUtilGlobal.h>

namespace AudioUtil
{
    using MarkerList = std::vector<std::pair<uint64_t, uint64_t>>;

    enum AUDIO_UTIL_EXPORT SlicerErrorCode { SLICER_OK = 0, SLICER_INVALID_ARGUMENT, SLICER_AUDIO_ERROR };

    class AUDIO_UTIL_EXPORT Slicer {
    private:
        double m_threshold;
        uint64_t m_hopSize;
        uint64_t m_winSize;
        uint64_t m_minLength;
        uint64_t m_minInterval;
        uint64_t m_maxSilKept;
        SlicerErrorCode m_errCode;
        std::string m_errMsg;
        SndfileHandle *m_decoder;

    public:
        explicit Slicer(SndfileHandle *decoder, double threshold = -40.0, uint64_t minLength = 5000,
                        uint64_t minInterval = 300, uint64_t hopSize = 20, uint64_t maxSilKept = 5000);
        MarkerList slice();
        SlicerErrorCode getErrorCode();
        std::string getErrorMsg();
    };
} // namespace AudioUtil

#endif // SLICER_H
