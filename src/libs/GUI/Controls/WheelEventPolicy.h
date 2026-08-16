#ifndef WHEELEVENTPOLICY_H
#define WHEELEVENTPOLICY_H

#include <QCoreApplication>
#include <QWheelEvent>
#include <QWidget>

enum class WheelEventPolicy {
    Handle,
    Consume,
    Pass,
};

class WheelEventPolicySupport {
public:
    explicit WheelEventPolicySupport(const WheelEventPolicy policy = WheelEventPolicy::Handle)
        : m_wheelEventPolicy(policy) {
    }

    [[nodiscard]] WheelEventPolicy wheelEventPolicy() const {
        return m_wheelEventPolicy;
    }

    void setWheelEventPolicy(const WheelEventPolicy policy) {
        m_wheelEventPolicy = policy;
    }

protected:
    [[nodiscard]] bool processWheelEventPolicy(QWidget *source, QWheelEvent *event) const {
        switch (m_wheelEventPolicy) {
            case WheelEventPolicy::Handle:
                return false;
            case WheelEventPolicy::Consume:
                event->ignore();
                return true;
            case WheelEventPolicy::Pass:
                forwardToParent(source, event);
                return true;
        }
        return false;
    }

private:
    static void forwardToParent(QWidget *source, QWheelEvent *event) {
        for (auto *parent = source->parentWidget(); parent; parent = parent->parentWidget()) {
            const QPointF position(parent->mapFromGlobal(event->globalPosition().toPoint()));
            QWheelEvent forwardedEvent(position, event->globalPosition(), event->pixelDelta(),
                                       event->angleDelta(), event->buttons(), event->modifiers(),
                                       event->phase(), event->inverted(), event->source(),
                                       event->pointingDevice());
            forwardedEvent.setTimestamp(event->timestamp());
            forwardedEvent.ignore();
            QCoreApplication::sendEvent(parent, &forwardedEvent);
            if (forwardedEvent.isAccepted())
                break;
        }
        event->accept();
    }

    WheelEventPolicy m_wheelEventPolicy;
};

#endif // WHEELEVENTPOLICY_H
