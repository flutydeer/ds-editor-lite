//
// Created by fluty on 24-3-21.
//

#include "AudioOption.h"

void AudioOption::load(const QJsonObject &object) {
    if (object.contains(hotPlugModeKey))
        hotPlugMode = hotPlugModeFromString(object.value(hotPlugModeKey).toString());
    if (object.contains(closeDeviceAtBackgroundKey))
        closeDeviceAtBackground = object.value(closeDeviceAtBackgroundKey).toBool();
    if (object.contains(closeDeviceOnPlaybackStopKey))
        closeDeviceOnPlaybackStop = object.value(closeDeviceOnPlaybackStopKey).toBool();
    if (object.contains(fileBufferingSizeMsecKey))
        fileBufferingSizeMsec = object.value(fileBufferingSizeMsecKey).toDouble();
}
AudioSystem::HotPlugMode AudioOption::hotPlugModeFromString(const QString &mode) {
    if (mode == "notifyOnCurrentRemoval")
        return AudioSystem::NotifyOnCurrentRemoval;

    if (mode == "none")
        return AudioSystem::None;

    if (mode == "notifyOnAnyChange")
        return AudioSystem::NotifyOnAnyChange;

    return AudioSystem::NotifyOnAnyChange;
}
QString AudioOption::hotPlugModeToString(const AudioSystem::HotPlugMode &mode) {
    if (mode == AudioSystem::NotifyOnCurrentRemoval)
        return "notifyOnCurrentRemoval";

    if (mode == AudioSystem::None)
        return "none";

    if (mode == AudioSystem::NotifyOnAnyChange)
        return "notifyOnAnyChange";

    return "notifyOnAnyChange";
}
void AudioOption::save(QJsonObject &object) {
    object.insert(hotPlugModeKey, hotPlugModeToString(hotPlugMode));
    object.insert(closeDeviceAtBackgroundKey, closeDeviceAtBackground);
    object.insert(closeDeviceOnPlaybackStopKey, closeDeviceOnPlaybackStop);
    object.insert(fileBufferingSizeMsecKey, fileBufferingSizeMsec);
}