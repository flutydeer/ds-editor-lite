#include "AudioSettings.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"

namespace {
    template <typename Mutation>
    void updateAudioSettings(Mutation mutation) {
        auto *runtime = AppContext::instance<Automation::CoreRuntime>();
        if (!runtime)
            return;
        const auto snapshot = runtime->settings().getSettings();
        if (!snapshot)
            return;
        auto settings = snapshot.get().audio;
        mutation(settings);
        runtime->settings().updateAudio({}, settings);
    }
}

// Indexed option keys are persisted machine-readable identifiers and must use C-locale digits.
#define AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_1(readName, writeName, dtoField)                \
    AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(                                                  \
        readName, writeName, std::invoke_result<decltype(&AudioSettings::readName)>::type{},       \
        dtoField)

#define AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(readName, writeName, defaultValue, dtoField)  \
    std::invoke_result<decltype(&AudioSettings::readName)>::type AudioSettings::readName() {       \
        using valueType = std::invoke_result<decltype(&AudioSettings::readName)>::type;            \
        auto variant = appOptions->audio()->obj[#readName].toVariant();                            \
        return variant.isNull() ? valueType(defaultValue) : variant.value<valueType>();            \
    }                                                                                              \
    void AudioSettings::writeName(                                                                 \
        const std::invoke_result<decltype(&AudioSettings::readName)>::type &v) {                   \
        updateAudioSettings(                                                                       \
            [&v](Automation::AudioSettingsDto &settings) { settings.dtoField = v; });              \
    }

#define AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_WITH_INDEX_1(readName, writeName, dtoField)     \
    AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_WITH_INDEX_2(                                       \
        readName, writeName, std::invoke_result<decltype(&AudioSettings::readName), int>::type{},  \
        dtoField)

#define AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_WITH_INDEX_2(readName, writeName, defaultValue, \
                                                                dtoField)                          \
    std::invoke_result<decltype(&AudioSettings::readName), int>::type AudioSettings::readName(     \
        int index) {                                                                               \
        using valueType = std::invoke_result<decltype(&AudioSettings::readName), int>::type;       \
        auto variant = appOptions->audio()->obj[QString::number(index) + #readName].toVariant();   \
        return variant.isNull() ? valueType(defaultValue) : variant.value<valueType>();            \
    }                                                                                              \
    void AudioSettings::writeName(                                                                 \
        int index, const std::invoke_result<decltype(&AudioSettings::readName), int>::type &v) {   \
        updateAudioSettings([index, &v](Automation::AudioSettingsDto &settings) {                  \
            if (index >= 0 && index < settings.pseudoSingerSynthesizers.size())                    \
                settings.pseudoSingerSynthesizers[index].dtoField = v;                             \
        });                                                                                        \
    }

#define AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_WITH_INDEX_3(readName, writeName, dtoField)     \
    std::invoke_result<decltype(&AudioSettings::readName), int>::type AudioSettings::readName(     \
        int index) {                                                                               \
        using valueType = std::invoke_result<decltype(&AudioSettings::readName), int>::type;       \
        auto variant = appOptions->audio()->obj[QString::number(index) + #readName].toVariant();   \
        return variant.isNull() ? readName##DefaultValue[index] : variant.value<valueType>();      \
    }                                                                                              \
    void AudioSettings::writeName(                                                                 \
        int index, const std::invoke_result<decltype(&AudioSettings::readName), int>::type &v) {   \
        updateAudioSettings([index, &v](Automation::AudioSettingsDto &settings) {                  \
            if (index >= 0 && index < settings.pseudoSingerSynthesizers.size())                    \
                settings.pseudoSingerSynthesizers[index].dtoField = v;                             \
        });                                                                                        \
    }

#include <Model/AppOptions/AppOptions.h>

AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_1(adoptedBufferSize, setAdoptedBufferSize,
                                             adoptedBufferSize)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_1(adoptedSampleRate, setAdoptedSampleRate,
                                             adoptedSampleRate)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(deviceGain, setDeviceGain, 1.0, deviceGain)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_1(deviceName, setDeviceName, deviceName)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_1(devicePan, setDevicePan, devicePan)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_1(driverName, setDriverName, driverName)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_1(fileBufferingReadAheadSize,
                                             setFileBufferingReadAheadSize,
                                             fileBufferingReadAheadSize)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_1(hotPlugNotificationMode, setHotPlugNotificationMode,
                                             hotPlugNotificationMode)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_1(playheadBehavior, setPlayheadBehavior,
                                             playheadBehavior)

AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(midiDeviceIndex, setMidiDeviceIndex, -1,
                                             midiDeviceIndex)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(midiSynthesizerAmplitude, setMidiSynthesizerAmplitude,
                                             -9, midiSynthesizerAmplitude)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(midiSynthesizerAttackMsec,
                                             setMidiSynthesizerAttackMsec, 10,
                                             midiSynthesizerAttackMilliseconds)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(midiSynthesizerDecayMsec, setMidiSynthesizerDecayMsec,
                                             1000, midiSynthesizerDecayMilliseconds)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(midiSynthesizerDecayRatio,
                                             setMidiSynthesizerDecayRatio, 0.5,
                                             midiSynthesizerDecayRatio)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_1(midiSynthesizerFrequencyOfA,
                                             setMidiSynthesizerFrequencyOfA,
                                             midiSynthesizerFrequencyOfA)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(midiSynthesizerGenerator, setMidiSynthesizerGenerator,
                                             3, midiSynthesizerGenerator)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(midiSynthesizerReleaseMsec,
                                             setMidiSynthesizerReleaseMsec, 50,
                                             midiSynthesizerReleaseMilliseconds)

AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(pseudoSingerReadEnergy, setPseudoSingerReadEnergy,
                                             true, pseudoSingerReadEnergy)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(pseudoSingerReadPitch, setPseudoSingerReadPitch, true,
                                             pseudoSingerReadPitch)
static double pseudoSingerSynthAmplitudeDefaultValue[] = {-12, -18, -12, -18};
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_WITH_INDEX_3(pseudoSingerSynthAmplitude,
                                                        setPseudoSingerSynthAmplitude, amplitude)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_WITH_INDEX_2(pseudoSingerSynthAttackMsec,
                                                        setPseudoSingerSynthAttackMsec, 50,
                                                        attackMilliseconds)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_WITH_INDEX_2(pseudoSingerSynthDecayMsec,
                                                        setPseudoSingerSynthDecayMsec, 1000,
                                                        decayMilliseconds)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_WITH_INDEX_2(pseudoSingerSynthDecayRatio,
                                                        setPseudoSingerSynthDecayRatio, 0.5,
                                                        decayRatio)
static int pseudoSingerSynthGeneratorDefaultValue[] = {0, 1, 2, 3};
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_WITH_INDEX_3(pseudoSingerSynthGenerator,
                                                        setPseudoSingerSynthGenerator, generator)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_WITH_INDEX_2(pseudoSingerSynthReleaseMsec,
                                                        setPseudoSingerSynthReleaseMsec, 50,
                                                        releaseMilliseconds)

AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(vstEditorPort, setVstEditorPort, 28081, vstEditorPort)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_1(vstPluginEditorUsesCustomTheme,
                                             setVstPluginEditorUsesCustomTheme,
                                             vstPluginEditorUsesCustomTheme)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(vstPluginPort, setVstPluginPort, 28082, vstPluginPort)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(audioExporterClippingCheckEnabled,
                                             setAudioExporterClippingCheckEnabled, true,
                                             audioExporterClippingCheckEnabled)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_1(audioExporterIgnoredWarningFlag,
                                             setAudioExporterIgnoredWarningFlag,
                                             audioExporterIgnoredWarningFlags)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(audioExporterUseTemporaryFile,
                                             setAudioExporterUseTemporaryFile, true,
                                             audioExporterUseTemporaryFile)
