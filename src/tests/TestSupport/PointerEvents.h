#pragma once

#include <QCoreApplication>
#include <QPointingDevice>
#include <QTabletEvent>
#include <QWidget>

namespace TestSupport {
    inline bool sendTabletEvent(QWidget &widget, const QPointingDevice &device, QEvent::Type type,
                                const QPoint &position, qreal pressure, Qt::MouseButton button,
                                Qt::MouseButtons buttons) {
        QTabletEvent event(type, &device, position, widget.mapToGlobal(position), pressure, 0, 0, 0,
                           0, 0, Qt::NoModifier, button, buttons);
        event.setAccepted(false);
        QCoreApplication::sendEvent(&widget, &event);
        return event.isAccepted();
    }
}
