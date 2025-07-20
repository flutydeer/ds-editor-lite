#ifndef RMVPE_H
#define RMVPE_H

#include <filesystem>
#include <functional>

#include <audio-util/SndfileVio.h>
#include <rmvpe-infer/RmvpeGlobal.h>
#include <rmvpe-infer/RmvpeModel.h>

namespace Rmvpe
{
    struct RmvpeRes {
        float offset; // ms
        std::vector<float> f0;
        std::vector<bool> uv;
    };

    class RMVPE_INFER_EXPORT Rmvpe {
    public:
        explicit Rmvpe(const srt::SynthUnit *su);
        ~Rmvpe();

        srt::Expected<void> open(const std::filesystem::path &modelPath);
        void close();

        bool is_open() const;

        bool get_f0(const std::filesystem::path &filepath, float threshold, std::vector<RmvpeRes> &res,
                    std::string &msg, const std::function<void(int)> &progressChanged);

        void terminate();

        RmvpeModel m_rmvpe;
    };
} // namespace Rmvpe

#endif // RMVPE_H
