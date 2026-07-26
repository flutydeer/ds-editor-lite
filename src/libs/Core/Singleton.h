//
// Created by fluty on 2024/1/31.
// Modified by Jobsecond on 2025/9/6.
//

#ifndef LITE_CORE_SINGLETON_H
#define LITE_CORE_SINGLETON_H

#include "Registry.h"

// The owner that constructs the (private-ctor) singletons and registers them.
// Forward-declared only: this header has no dependency on its definition, so it
// lives in lite::Core and any layer may use LITE_SINGLETON.
class AppContext;

// Put this in the class definition (.h)
#define LITE_SINGLETON_DECLARE_INSTANCE(ClassName)                                                 \
    friend class AppContext;                                                                        \
    static ClassName *instance();

// Put this in the source file (.cpp).
// Looks the instance up in the process Registry (populated by whoever owns the
// object, e.g. the app's AppContext). Falls back to a Meyers static when nothing
// is registered — e.g. in tests, or for a library singleton that has no
// managing owner.
#define LITE_SINGLETON_IMPLEMENT_INSTANCE(ClassName)                                               \
    ClassName *ClassName::instance() {                                                             \
        if (auto *p = Registry::instance<ClassName>())                                             \
            return p;                                                                               \
        static ClassName obj;                                                                       \
        return &obj;                                                                                \
    }

#endif // LITE_CORE_SINGLETON_H
