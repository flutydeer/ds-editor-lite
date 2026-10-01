#pragma once

#include <TalcsFormat/AudioFormatIO.h>

namespace TestSupport {
    class UnavailableAudioBackend final : public talcs::AudioFormatIO {
    public:
        bool open(OpenMode) override {
            return false;
        }
    };
}
