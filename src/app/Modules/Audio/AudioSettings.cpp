#include "AudioSettings.h"

#define AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_1(readName, writeName)                          \
  AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(                                                    \
      readName, writeName, std::invoke_result<decltype(&AudioSettings::readName)>::type{})

#define AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(readName, writeName, defaultValue)            \
  std::invoke_result<decltype(&AudioSettings::readName)>::type AudioSettings::readName() {         \
    using valueType = std::invoke_result<decltype(&AudioSettings::readName)>::type;                \
    auto variant = appOptions->audio()->obj[#readName].toVariant();                                \
    return variant.isNull() ? valueType(defaultValue) : variant.value<valueType>();                \
  }                                                                                                \
  void AudioSettings::writeName(                                                                   \
      const std::invoke_result<decltype(&AudioSettings::readName)>::type &v) {                     \
    appOptions->audio()->obj[#readName] = v;                                                       \
  }

#define AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_WITH_INDEX_1(readName, writeName)               \
  AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_WITH_INDEX_2(                                         \
      readName, writeName, std::invoke_result<decltype(&AudioSettings::readName), int>::type{})

#define AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_WITH_INDEX_2(readName, writeName, defaultValue) \
  std::invoke_result<decltype(&AudioSettings::readName), int>::type AudioSettings::readName(       \
      int index) {                                                                                 \
    using valueType = std::invoke_result<decltype(&AudioSettings::readName), int>::type;           \
    auto variant = appOptions->audio()->obj[QString::number(index) + #readName].toVariant();       \
    return variant.isNull() ? valueType(defaultValue) : variant.value<valueType>();                \
  }                                                                                                \
  void AudioSettings::writeName(                                                                   \
      int index, const std::invoke_result<decltype(&AudioSettings::readName), int>::type &v) {     \
    appOptions->audio()->obj[QString::number(index) + #readName] = v;                              \
  }

#define AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_WITH_INDEX_3(readName, writeName)               \
  std::invoke_result<decltype(&AudioSettings::readName), int>::type AudioSettings::readName(       \
      int index) {                                                                                 \
    using valueType = std::invoke_result<decltype(&AudioSettings::readName), int>::type;           \
    auto variant = appOptions->audio()->obj[QString::number(index) + #readName].toVariant();       \
    return variant.isNull() ? readName##DefaultValue[index] : variant.value<valueType>();          \
  }                                                                                                \
  void AudioSettings::writeName(                                                                   \
      int index, const std::invoke_result<decltype(&AudioSettings::readName), int>::type &v) {     \
    appOptions->audio()->obj[QString::number(index) + #readName] = v;                              \
  }

#define AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_QJSONVALUE(readName, writeName)                 \
    QJsonValue AudioSettings::readName() {                                                         \
        return appOptions->audio()->obj[#readName];                                                \
    }                                                                                              \
    void AudioSettings::writeName(const QJsonValue &v) {                                           \
        appOptions->audio()->obj[#readName] = v;                                                   \
    }


#include <Model/AppOptions/AppOptions.h>

AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_1(adoptedBufferSize, setAdoptedBufferSize)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_1(adoptedSampleRate, setAdoptedSampleRate)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(deviceGain, setDeviceGain, 1.0)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_1(deviceName, setDeviceName)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_1(devicePan, setDevicePan)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_1(driverName, setDriverName)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_1(fileBufferingReadAheadSize, setFileBufferingReadAheadSize)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_1(hotPlugNotificationMode, setHotPlugNotificationMode)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_1(playheadBehavior, setPlayheadBehavior)

AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(midiDeviceIndex, setMidiDeviceIndex, -1)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(midiSynthesizerAmplitude, setMidiSynthesizerAmplitude, -9)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(midiSynthesizerAttackMsec, setMidiSynthesizerAttackMsec, 10)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(midiSynthesizerDecayMsec, setMidiSynthesizerDecayMsec, 1000)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(midiSynthesizerDecayRatio, setMidiSynthesizerDecayRatio, 0.5)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_1(midiSynthesizerFrequencyOfA, setMidiSynthesizerFrequencyOfA)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(midiSynthesizerGenerator, setMidiSynthesizerGenerator, 3)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(midiSynthesizerReleaseMsec, setMidiSynthesizerReleaseMsec, 50)

AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(pseudoSingerReadEnergy, setPseudoSingerReadEnergy, true)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(pseudoSingerReadPitch, setPseudoSingerReadPitch, true)
static double pseudoSingerSynthAmplitudeDefaultValue[] = {-12, -18, -12, -18};
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_WITH_INDEX_3(pseudoSingerSynthAmplitude, setPseudoSingerSynthAmplitude)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_WITH_INDEX_2(pseudoSingerSynthAttackMsec, setPseudoSingerSynthAttackMsec, 50)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_WITH_INDEX_2(pseudoSingerSynthDecayMsec, setPseudoSingerSynthDecayMsec, 1000)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_WITH_INDEX_2(pseudoSingerSynthDecayRatio, setPseudoSingerSynthDecayRatio, 0.5)
static int pseudoSingerSynthGeneratorDefaultValue[] = {0, 1, 2, 3};
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_WITH_INDEX_3(pseudoSingerSynthGenerator, setPseudoSingerSynthGenerator)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_WITH_INDEX_2(pseudoSingerSynthReleaseMsec, setPseudoSingerSynthReleaseMsec, 50)

AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(vstEditorPort, setVstEditorPort, 28081)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_1(vstPluginEditorUsesCustomTheme, setVstPluginEditorUsesCustomTheme)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(vstPluginPort, setVstPluginPort, 28082)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_QJSONVALUE(vstTheme, setVstTheme)

AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_QJSONVALUE(audioExporterPresets, setAudioExporterPresets)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(audioExporterClippingCheckEnabled, setAudioExporterClippingCheckEnabled, true)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_QJSONVALUE(audioExporterCurrentPreset, setAudioExporterCurrentPreset)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_1(audioExporterIgnoredWarningFlag, setAudioExporterIgnoredWarningFlag)
AUDIO_AUDIO_SETTINGS_OPTION_IMPLEMENTATION_2(audioExporterUseTemporaryFile, setAudioExporterUseTemporaryFile, true)