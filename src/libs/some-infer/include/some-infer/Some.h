#ifndef RMVPE_H
#define RMVPE_H

#include <filesystem>

#include <audio-util/SndfileVio.h>
#include <some-infer/Provider.h>
#include <some-infer/SomeGlobal.h>

namespace Some
{
    class SomeModel;
    class Some {
    public:
        explicit SOME_INFER_EXPORT Some(const std::filesystem::path &modelPath, ExecutionProvider provider,
                                        int device_id);
        SOME_INFER_EXPORT ~Some();

        bool SOME_INFER_EXPORT get_midi(const std::filesystem::path &filepath, std::vector<float> &note_midi,
                                        std::vector<bool> &note_rest, std::vector<float> &note_dur, std::string &msg,
                                        void (*progressChanged)(int)) const;

    private:
        bool get_midi(AudioUtil::SF_VIO sf_vio, std::vector<float> &note_midi, std::vector<bool> &note_rest,
                      std::vector<float> &note_dur, std::string &msg, void (*progressChanged)(int)) const;

        std::unique_ptr<SomeModel> m_some;
    };
} // namespace Some

#endif // RMVPE_H
