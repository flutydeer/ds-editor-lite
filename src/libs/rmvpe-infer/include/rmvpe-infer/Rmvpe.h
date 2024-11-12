#ifndef RMVPE_H
#define RMVPE_H

#include <filesystem>

#include <rmvpe-infer/RmvpeModel.h>
#include <rmvpe-infer/SndfileVio.h>

#include <rmvpe-infer/RmvpeGlobal.h>

namespace Rmvpe
{
    class Rmvpe {
    public:
        explicit RMVPE_INFER_EXPORT Rmvpe(const std::filesystem::path &modelPath);
        RMVPE_INFER_EXPORT ~Rmvpe();

        bool RMVPE_INFER_EXPORT get_f0(const std::filesystem::path &filepath, float threshold, std::vector<float> &f0,
                                       std::vector<bool> &uv, std::string &msg) const;

    private:
        bool get_f0(SF_VIO sf_vio, float threshold, std::vector<float> &f0, std::vector<bool> &uv,
                    std::string &msg) const;

        static SF_VIO resample(const std::filesystem::path &filepath);
        std::unique_ptr<RmvpeModel> m_rmvpe;
    };
} // namespace Rmvpe

#endif // RMVPE_H
