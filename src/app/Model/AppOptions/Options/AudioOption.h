//
// Created by fluty on 24-3-21.
//

#ifndef AUDIOOPTION_H
#define AUDIOOPTION_H

#include <TalcsDevice/OutputContext.h>
#include <TalcsCore/NoteSynthesizer.h>

#include "Model/AppOptions/IOption.h"
#include "Modules/Audio/AudioSystem.h"

class AudioOption : public IOption {
public:
    explicit AudioOption() : IOption("audio"){}

    void load(const QJsonObject &object) override;

    QString driverName;
    QString deviceName;
    double adoptedSampleRate{};
    qint64 adoptedBufferSize{};
    double deviceGain = 1.0;
    double devicePan{};
    talcs::OutputContext::HotPlugNotificationMode hotPlugNotificationMode{};
    double fileBufferingReadAheadSize{};

    quint16 vstEditorPort = 28081;
    quint16 vstPluginPort = 28082;
    bool vstPluginEditorUsesCustomTheme{};
    QJsonObject vstTheme{};

    int midiDeviceIndex{};
    talcs::NoteSynthesizer::Generator midiSynthesizerGenerator{};
    int midiSynthesizerAttackMsec = 10;
    int midiSynthesizerReleaseMsec = 50;
    double midiSynthesizerAmplitude = -3.0;
    double midiSynthesizerFrequencyOfA{};

protected:
    void save(QJsonObject &object) override;

};



#endif //AUDIOOPTION_H
