//
// Created by fluty on 24-3-21.
//

#include "AudioOption.h"

void AudioOption::load(const QJsonObject &object) {
    driverName = object.value("driverName").toString();
    deviceName = object.value("deviceName").toString();
    adoptedBufferSize = object.value("adoptedBufferSize").toInt();
    adoptedSampleRate = object.value("adoptedSampleRate").toDouble();
    deviceGain = object.value("deviceGain").toDouble(1.0);
    devicePan = object.value("devicePan").toDouble();
    hotPlugNotificationMode = static_cast<talcs::OutputContext::HotPlugNotificationMode>(object.value("hotPlugNotificationMode").toInt());
    fileBufferingReadAheadSize = object.value("fileBufferingReadAheadSize").toInt();

    vstEditorPort = static_cast<quint16>(object.value("vstEditorPort").toInt(28081));
    vstPluginPort = static_cast<quint16>(object.value("vstPluginPort").toInt(28082));
    vstPluginEditorUsesCustomTheme = object.value("vstPluginEditorUsesCustomTheme").toBool();
    vstTheme = object.value("vstTheme").toObject();

    midiDeviceIndex = object.value("midiDeviceIndex").toInt();
    midiSynthesizerGenerator = static_cast<talcs::NoteSynthesizer::Generator>(object.value("midiSynthesizerGenerator").toInt());
    midiSynthesizerAttackMsec = object.value("midiSynthesizerAttackMsec").toInt(10);
    midiSynthesizerReleaseMsec = object.value("midiSynthesizerReleaseMsec").toInt(50);
    midiSynthesizerAmplitude = object.value("midiSynthesizerAmplitude").toDouble(-3.0);
    midiSynthesizerFrequencyOfA = object.value("midiSynthesizerFrequencyOfA").toDouble();

}
void AudioOption::save(QJsonObject &object) {
    object.insert("driverName", driverName);
    object.insert("deviceName", deviceName);
    object.insert("adoptedBufferSize", adoptedBufferSize);
    object.insert("adoptedSampleRate", adoptedSampleRate);
    object.insert("deviceGain", deviceGain);
    object.insert("devicePan", devicePan);
    object.insert("hotPlugNotificationMode", hotPlugNotificationMode);
    object.insert("fileBufferingReadAheadSize", fileBufferingReadAheadSize);
    object.insert("vstEditorPort", vstEditorPort);
    object.insert("vstPluginPort", vstPluginPort);
    object.insert("vstPluginEditorUsesCustomTheme", vstPluginEditorUsesCustomTheme);
    object.insert("vstTheme", vstTheme);
    object.insert("midiDeviceIndex", midiDeviceIndex);
    object.insert("midiSynthesizerGenerator", midiSynthesizerGenerator);
    object.insert("midiSynthesizerAttackMsec", midiSynthesizerAttackMsec);
    object.insert("midiSynthesizerReleaseMsec", midiSynthesizerReleaseMsec);
    object.insert("midiSynthesizerAmplitude", midiSynthesizerAmplitude);
    object.insert("midiSynthesizerFrequencyOfA", midiSynthesizerFrequencyOfA);
}