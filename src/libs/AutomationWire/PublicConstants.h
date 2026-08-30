#ifndef AUTOMATIONWIRE_PUBLICCONSTANTS_H
#define AUTOMATIONWIRE_PUBLICCONSTANTS_H

namespace AutomationWire {

    inline constexpr int TrackPaletteColorCount = 12;

    inline constexpr double MinimumPan = -1.0;
    inline constexpr double MaximumPan = 1.0;
    inline constexpr double MaximumAudioDeviceGain = 1.9952623149688795;
    inline constexpr double MinimumMixWeight = 0.0;
    inline constexpr double MaximumMixWeight = 1.0;

    inline constexpr int MinimumMidiKeyIndex = 0;
    inline constexpr int MaximumMidiKeyIndex = 127;
    inline constexpr int MinimumCentShift = -100;
    inline constexpr int MaximumCentShift = 100;

    inline constexpr int MinimumAudioSampleRate = 8000;
    inline constexpr int MaximumAudioSampleRate = 384000;
    inline constexpr int MinimumPageSize = 1;
    inline constexpr int MaximumPageSize = 1000;
    inline constexpr int MaximumIdempotencyKeyLength = 128;
    inline constexpr int MaximumCommandCollectionItems = 1024;
    inline constexpr int MaximumCurveSampleItems = 65536;
    inline constexpr int MaximumAudioImportBatchItems = 64;

}

#endif // AUTOMATIONWIRE_PUBLICCONSTANTS_H
