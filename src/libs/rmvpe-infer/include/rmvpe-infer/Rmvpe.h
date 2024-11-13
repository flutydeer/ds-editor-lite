#ifndef RMVPE_H
#define RMVPE_H

#include <filesystem>

#include <audio-util/SndfileVio.h>
#include <rmvpe-infer/Provider.h>
#include <rmvpe-infer/RmvpeGlobal.h>

namespace Rmvpe
{
    class RmvpeModel;
    class Rmvpe {
    public:
        explicit RMVPE_INFER_EXPORT Rmvpe(const std::filesystem::path &modelPath, ExecutionProvider provider,
                                          int device_id);
        RMVPE_INFER_EXPORT ~Rmvpe();

        bool RMVPE_INFER_EXPORT get_f0(const std::filesystem::path &filepath, float threshold, std::vector<float> &f0,
                                       std::vector<bool> &uv, std::string &msg, void (*progressChanged)(int)) const;

    private:
        bool get_f0(AudioUtil::SF_VIO sf_vio, float threshold, std::vector<float> &f0, std::vector<bool> &uv,
                    std::string &msg, void (*progressChanged)(int)) const;

        std::unique_ptr<RmvpeModel> m_rmvpe;
    };
} // namespace Rmvpe

#endif // RMVPE_H
