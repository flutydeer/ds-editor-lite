#ifndef EDITORWHEELUTILS_H
#define EDITORWHEELUTILS_H

#include <QInputDevice>
#include <QTimer>
#include <QWheelEvent>

#include <cmath>

namespace EditorWheelUtils {

    inline double axisValue(const QPoint &delta, const Qt::Orientation axis) {
        return axis == Qt::Horizontal ? delta.x() : delta.y();
    }

    inline bool usesPixelDelta(const QWheelEvent *event) {
        if (event->pixelDelta().isNull())
            return false;
        return event->angleDelta().isNull() ||
               event->deviceType() == QInputDevice::DeviceType::TouchPad;
    }

    inline double wheelDelta(const QWheelEvent *event, const Qt::Orientation axis) {
        if (usesPixelDelta(event))
            return axisValue(event->pixelDelta(), axis) * 4.0;
        return axisValue(event->angleDelta(), axis);
    }

    inline Qt::Orientation dominantAxis(const QWheelEvent *event) {
        const auto delta = usesPixelDelta(event) ? event->pixelDelta() : event->angleDelta();
        return qAbs(delta.x()) > qAbs(delta.y()) ? Qt::Horizontal : Qt::Vertical;
    }

    inline Qt::Orientation horizontalScrollAxis(const QWheelEvent *event) {
        return event->modifiers() == Qt::ShiftModifier ? Qt::Vertical : Qt::Horizontal;
    }

    class ScrollAccumulator final {
    public:
        [[nodiscard]] int scrollTarget(const int startValue, const int viewportLength,
                                       const double viewportFraction, const QWheelEvent *event,
                                       const Qt::Orientation axis) {
            if (event->phase() == Qt::ScrollBegin)
                reset();

            if (usesPixelDelta(event)) {
                reset();
                return startValue - axisValue(event->pixelDelta(), axis);
            }

            const auto wholeStep = static_cast<int>(viewportLength * viewportFraction);
            const auto offset = -wholeStep * axisValue(event->angleDelta(), axis) / 120.0;
            if (!qFuzzyIsNull(m_remainder) && !qFuzzyIsNull(offset) &&
                std::signbit(m_remainder) != std::signbit(offset)) {
                reset();
            }
            m_remainder += offset;
            const auto wholeOffset = static_cast<int>(m_remainder);
            m_remainder -= wholeOffset;
            const auto target = startValue + wholeOffset;

            if (event->phase() == Qt::ScrollEnd)
                reset();
            return target;
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
            if (usesPixelDelta(event))
                return false;
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
            if (usesPixelDelta(event)) {
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
