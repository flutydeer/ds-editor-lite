#ifndef SOME_H
#define SOME_H

#include <filesystem>
#include <functional>

#include <audio-util/SndfileVio.h>
#include <some-infer/SomeGlobal.h>
#include <some-infer/SomeModel.h>

#include <synthrt/Support/Expected.h>

namespace Some
{
    struct SOME_INFER_EXPORT Midi {
        int note, start, duration;
    };

    class SOME_INFER_EXPORT Some {
    public:
        explicit Some(const srt::SynthUnit *su);
        ~Some();

        srt::Expected<void> open(const std::filesystem::path &modelPath);
        void close();

        bool is_open() const;

        bool get_midi(const std::filesystem::path &filepath, std::vector<Midi> &midis, float tempo, std::string &msg,
                      const std::function<void(int)> &progressChanged);

        void terminate();

    private:
        // bool get_midi(AudioUtil::SF_VIO sf_vio, std::vector<Midi> &midis, float tempo, std::string &msg,
        //               const std::function<void(int)> &progressChanged) const;

        SomeModel m_some;
    };
} // namespace Some

#endif // SOME_H
