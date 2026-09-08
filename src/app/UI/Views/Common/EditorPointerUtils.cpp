#include "EditorPointerUtils.h"

#include "Global/AppGlobal.h"

#include <QGuiApplication>
#include <QPointerEvent>

namespace {
    // Nesting is possible in principle (a queued context menu opening while a
    // second view still holds a stream), so this is a counter rather than a
    // flag.
    int g_touchStreamDepth = 0;
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

    bool isPointerPressed() {
        return QGuiApplication::mouseButtons() != Qt::NoButton || isTouchStreamActive();
    }

    double resizeTolerance() {
        return resizeTolerance(AppGlobal::resizeTolerance);
    }

    double resizeTolerance(const double baseTolerance) {
        return isTouchStreamActive() ? baseTolerance * touchToleranceScale : baseTolerance;
    }

}
