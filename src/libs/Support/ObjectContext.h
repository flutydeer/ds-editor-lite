#pragma once

#include <lite/Core/SingletonRegistry.h>
#include <QObject>
#include <QVariant>
#include <functional>

namespace ObjectContext {
    inline constexpr char propertyName[] = "_lite_service_context";

    inline SingletonRegistry::Context of(const QObject *object) {
        for (; object; object = object->parent()) {
            const auto context = object->property(propertyName);
            if (context.isValid())
                return context.value<SingletonRegistry::Context>();
        }
        return {};
    }

    inline void bind(QObject *object) {
        object->setProperty(propertyName, QVariant::fromValue(SingletonRegistry::current()));
    }

    template <class T, class... Args>
    T *create(Args &&...args) {
        T *object = SingletonRegistry::create<T>(std::forward<Args>(args)...);
        bind(object);
        return object;
    }

    // A process-wide sender can call receivers in different documents synchronously.
    template <class Sender, class Owner, class... Args, class Receiver, class Slot>
    QMetaObject::Connection connect(Sender *sender, void (Owner::*signal)(Args...),
                                    Receiver *receiver, Slot slot) {
        return QObject::connect(
            sender, signal, receiver,
            [context = SingletonRegistry::current(), receiver, slot](Args... args) {
                const SingletonRegistry::Scope scope(context);
                if constexpr (std::is_member_function_pointer_v<Slot>) {
                    if constexpr (std::is_invocable_v<Slot, Receiver *, Args...>)
                        std::invoke(slot, receiver, args...);
                    else
                        std::invoke(slot, receiver);
                } else if constexpr (std::is_invocable_v<Slot, Args...>)
                    std::invoke(slot, args...);
                else
                    std::invoke(slot);
            });
    }
}
