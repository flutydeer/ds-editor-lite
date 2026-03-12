#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "GameGlobal.h"
#include "GameModel.h"
#include "synthrt/Support/Expected.h"

namespace srt
{
    class SynthUnit;
}

namespace Game
{

    struct GameMidi {
        int note;
        int start;
        int duration;
    };

    class GameModel; // Forward declaration

    class GAME_INFER_EXPORT Game {
    public:
        explicit Game(const srt::SynthUnit *su);
        ~Game();
        srt::Expected<void> open(const std::filesystem::path &modelPath);

        bool is_open() const;
        void terminate() const;

        bool get_midi(const std::filesystem::path &filepath, std::vector<GameMidi> &midis, float tempo,
                      std::string &msg, const std::function<void(int)> &progressChanged) const;

        // Methods to update model parameters
        void set_seg_threshold(float threshold);
        void set_seg_radius_seconds(float radius);
        void set_seg_radius_frames(float radiusFrames);
        void set_est_threshold(float threshold);
        void set_d3pm_ts(const std::vector<float> &ts);
        void set_language(int language);

    private:
        GameModel su;
    };
} // namespace Game
