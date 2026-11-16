#ifndef DEVICELOCKER_H
#define DEVICELOCKER_H

#include <TalcsDevice/AudioDevice.h>

// Device locks exclude the render callback while model updates run. They are
// not reentrant, but model signals are emitted inside a locked scope and their
// handlers lock again. The output device is a process-wide singleton, so nested
// scopes on one thread only track depth and the outermost scope owns the lock.
class DeviceLocker {
public:
    explicit DeviceLocker(talcs::AudioDevice *device);
    ~DeviceLocker();

    DeviceLocker(const DeviceLocker &) = delete;
    DeviceLocker &operator=(const DeviceLocker &) = delete;
};

#endif // DEVICELOCKER_H
