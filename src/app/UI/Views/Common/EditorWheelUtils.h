#ifndef EDITORWHEELUTILS_H
#define EDITORWHEELUTILS_H

#include <QInputDevice>
#include <QTimer>
#include <QWheelEvent>

#include <cmath>

namespace EditorWheelUtils {

    inline double wheelDelta(const QWheelEvent *event, const Qt::Orientation axis) {
        const auto pixelDelta = event->pixelDelta();
        if (!pixelDelta.isNull()) {
            const auto pixelValue = axis == Qt::Horizontal ? pixelDelta.x() : pixelDelta.y();
            return pixelValue * 4.0;
        }

        const auto angleDelta = event->angleDelta();
        const auto angleValue = axis == Qt::Horizontal ? angleDelta.x() : angleDelta.y();
        return angleValue;
    }

    inline Qt::Orientation dominantAxis(const QWheelEvent *event) {
        return qAbs(wheelDelta(event, Qt::Horizontal)) >
                       qAbs(wheelDelta(event, Qt::Vertical))
                   ? Qt::Horizontal
                   : Qt::Vertical;
    }

    inline double horizontalScrollDelta(const QWheelEvent *event) {
        const auto sourceAxis =
            event->modifiers() == Qt::ShiftModifier ? Qt::Vertical : Qt::Horizontal;
        return wheelDelta(event, sourceAxis);
    }

    inline int scrollTarget(const int startValue, const int viewportLength,
                            const double viewportFraction, const double wheelDelta) {
        return static_cast<int>(startValue -
                                viewportLength * viewportFraction * wheelDelta / 120.0);
    }

    class ScrollAccumulator final {
    public:
        int scrollTarget(const int startValue, const int viewportLength,
                         const double viewportFraction, const double wheelDelta) {
            const auto wholeStep = static_cast<int>(viewportLength * viewportFraction);
            const auto offset = -wholeStep * wheelDelta / 120.0;
            if (!qFuzzyIsNull(m_remainder) && !qFuzzyIsNull(offset) &&
                std::signbit(m_remainder) != std::signbit(offset)) {
                m_remainder = 0.0;
            }
            m_remainder += offset;
            const auto wholeOffset = static_cast<int>(m_remainder);
            m_remainder -= wholeOffset;
            return startValue + wholeOffset;
        }

        void reset() {
            m_remainder = 0.0;
        }

    private:
        double m_remainder = 0.0;
    };

    class InputState final {
    public:
        InputState() {
#if !defined(Q_OS_MAC) || QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            m_unlockTimer.setInterval(400);
            m_unlockTimer.setSingleShot(true);
            QObject::connect(&m_unlockTimer, &QTimer::timeout,
                             [this] { m_touchPadLocked = false; });
#endif
        }

        [[nodiscard]] bool isMouseWheel(const QWheelEvent *event) {
#if defined(Q_OS_MAC) && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            return event->deviceType() == QInputDevice::DeviceType::Mouse;
#else
            const auto deltaX = event->angleDelta().x();
            const auto deltaY = event->angleDelta().y();
            const auto absDx = qAbs(deltaX);
            const auto absDy = qAbs(deltaY);
            if (m_touchPadLocked) {
                m_unlockTimer.start();
                return false;
            }
            if (!event->pixelDelta().isNull()) {
                m_touchPadLocked = true;
                m_unlockTimer.start();
                return false;
            }
            if ((absDx == 0 && absDy % 120 == 0) || (absDx % 120 == 0 && absDy == 0))
                return true;
            m_touchPadLocked = true;
            m_unlockTimer.start();
            return false;
#endif
        }

    private:
#if !defined(Q_OS_MAC) || QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        QTimer m_unlockTimer;
        bool m_touchPadLocked = false;
#endif
    };

}

#endif // EDITORWHEELUTILS_H
