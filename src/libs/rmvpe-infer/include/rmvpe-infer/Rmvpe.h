#ifndef RMVPE_H
#define RMVPE_H

#include <filesystem>
#include <functional>

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
                                       std::vector<bool> &uv, std::string &msg,
                                       const std::function<void(int)> &progressChanged);

        void RMVPE_INFER_EXPORT terminate();

    private:
        bool get_f0(AudioUtil::SF_VIO sf_vio, float threshold, std::vector<float> &f0, std::vector<bool> &uv,
                    std::string &msg, const std::function<void(int)> &progressChanged);

        bool terminated = false;
        std::unique_ptr<RmvpeModel> m_rmvpe;
    };
} // namespace Rmvpe

#endif // RMVPE_H
