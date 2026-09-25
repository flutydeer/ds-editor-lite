#include "EditorPointerUtils.h"

#include "Global/AppGlobal.h"

#include <QGuiApplication>
#include <QPointerEvent>

namespace {
    // Nesting is possible in principle (a queued context menu opening while a
    // second view still holds a stream), so these are counters rather than
    // flags.
    int g_touchStreamDepth = 0;
    int g_penStreamDepth = 0;
    int g_penEraseIntentDepth = 0;
}

namespace EditorPointer {

    bool isTouchDevice(const QPointerEvent *event) {
        if (!event)
            return false;
        const auto *device = event->pointingDevice();
        if (!device)
            return false;
        const auto type = device->type();
        return type == QInputDevice::DeviceType::TouchScreen ||
               type == QInputDevice::DeviceType::TouchPad;
    }

    bool isPenDevice(const QPointerEvent *event) {
        if (!event)
            return false;
        const auto *device = event->pointingDevice();
        if (!device)
            return false;
        const auto type = device->type();
        return type == QInputDevice::DeviceType::Stylus ||
               type == QInputDevice::DeviceType::Airbrush ||
               type == QInputDevice::DeviceType::Puck;
    }

    bool isTouchStreamActive() {
        return g_touchStreamDepth > 0;
    }

    void beginTouchStream() {
        ++g_touchStreamDepth;
    }

    void endTouchStream() {
        if (g_touchStreamDepth > 0)
            --g_touchStreamDepth;
    }

    bool isPenStreamActive() {
        return g_penStreamDepth > 0;
    }

    void beginPenStream() {
        ++g_penStreamDepth;
    }

    void endPenStream() {
        if (g_penStreamDepth > 0)
            --g_penStreamDepth;
    }

    bool isPenEraseIntentActive() {
        return g_penEraseIntentDepth > 0;
    }

    void beginPenEraseIntent() {
        ++g_penEraseIntentDepth;
    }

    void endPenEraseIntent() {
        if (g_penEraseIntentDepth > 0)
            --g_penEraseIntentDepth;
    }

    bool isPointerPressed() {
        return QGuiApplication::mouseButtons() != Qt::NoButton || isTouchStreamActive() ||
               isPenStreamActive();
    }

    double resizeTolerance() {
        return resizeTolerance(AppGlobal::resizeTolerance);
    }

    double resizeTolerance(const double baseTolerance) {
        return isTouchStreamActive() ? baseTolerance * touchToleranceScale : baseTolerance;
    }

}
