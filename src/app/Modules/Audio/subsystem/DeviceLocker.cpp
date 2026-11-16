#include "DeviceLocker.h"

namespace {
    thread_local int s_depth = 0;
    thread_local talcs::AudioDevice *s_lockedDevice = nullptr;
}

DeviceLocker::DeviceLocker(talcs::AudioDevice *device) {
    if (s_depth == 0 && device) {
        device->lock();
        s_lockedDevice = device;
    }
    ++s_depth;
}

DeviceLocker::~DeviceLocker() {
    --s_depth;
    if (s_depth == 0 && s_lockedDevice) {
        s_lockedDevice->unlock();
        s_lockedDevice = nullptr;
    }
}
