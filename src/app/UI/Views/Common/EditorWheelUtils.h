#ifndef EDITORWHEELUTILS_H
#define EDITORWHEELUTILS_H

#include <QApplication>
#include <QInputDevice>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <QWheelEvent>

#include <functional>
#include <utility>

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

    inline int scrollTarget(const int startValue, const int viewportLength,
                            const double viewportFraction, const QWheelEvent *event,
                            const Qt::Orientation axis) {
        if (usesPixelDelta(event))
            return static_cast<int>(startValue - axisValue(event->pixelDelta(), axis));
        return static_cast<int>(startValue - viewportLength * viewportFraction *
                                                 axisValue(event->angleDelta(), axis) / 120.0);
    }

    namespace Detail {

        class ChildWheelEventFilter final : public QObject {
        public:
            ChildWheelEventFilter(QWidget *target, QObject *parent,
                                  std::function<void(QWheelEvent *)> handler)
                : QObject(parent), m_target(target), m_handler(std::move(handler)) {
                qApp->installEventFilter(this);
            }

        protected:
            bool eventFilter(QObject *watched, QEvent *event) override {
                if (event->type() != QEvent::Wheel || !m_target)
                    return false;

                auto *source = qobject_cast<QWidget *>(watched);
                if (!source || source == m_target || source->window() != m_target->window() ||
                    !m_target->isAncestorOf(source)) {
                    return false;
                }

                auto *wheelEvent = static_cast<QWheelEvent *>(event);
                QWheelEvent mappedEvent(
                    m_target->mapFromGlobal(wheelEvent->globalPosition()),
                    wheelEvent->globalPosition(), wheelEvent->pixelDelta(), wheelEvent->angleDelta(),
                    wheelEvent->buttons(), wheelEvent->modifiers(), wheelEvent->phase(),
                    wheelEvent->inverted(), wheelEvent->source(), wheelEvent->pointingDevice());
                mappedEvent.ignore();
                m_handler(&mappedEvent);
                if (!mappedEvent.isAccepted())
                    return false;

                event->accept();
                return true;
            }

        private:
            QPointer<QWidget> m_target;
            std::function<void(QWheelEvent *)> m_handler;
        };

    } // namespace Detail

    inline void forwardChildWheelEvents(QWidget *target, QObject *owner,
                                        std::function<void(QWheelEvent *)> handler) {
        new Detail::ChildWheelEventFilter(target, owner, std::move(handler));
    }

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
