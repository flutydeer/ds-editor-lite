//
// Created by fluty on 24-3-21.
//

#ifndef AUDIOOPTION_H
#define AUDIOOPTION_H

#include "Model/AppOptions/IOption.h"
#include "Modules/Audio/AudioSystem.h"

class AudioOption : public IOption {
public:
    explicit AudioOption() : IOption("audio"){}

    void load(const QJsonObject &object) override;

    QString driverName;
    QString deviceName;
    double adoptedSampleRate = 0;
    qint64 adoptedBufferSize = 0;

    AudioSystem::HotPlugMode hotPlugMode = AudioSystem::NotifyOnAnyChange;

    bool closeDeviceAtBackground = false;
    bool closeDeviceOnPlaybackStop = false;

    double fileBufferingSizeMsec = 1000.0;

    static AudioSystem::HotPlugMode hotPlugModeFromString(const QString &mode);
    static QString hotPlugModeToString(const AudioSystem::HotPlugMode &mode);

protected:
    void save(QJsonObject &object) override;

private:
    const QString hotPlugModeKey = "hotPlugMode";
    const QString closeDeviceAtBackgroundKey = "closeDeviceAtBackground";
    const QString closeDeviceOnPlaybackStopKey = "closeDeviceOnPlaybackStop";
    const QString fileBufferingSizeMsecKey = "fileBufferingSizeMsec";
};



#endif //AUDIOOPTION_H
