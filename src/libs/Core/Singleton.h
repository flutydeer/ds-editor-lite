//
// Created by fluty on 2024/1/31.
// Modified by Jobsecond on 2025/9/6.
//

#ifndef LITE_CORE_SINGLETON_H
#define LITE_CORE_SINGLETON_H

#include "SingletonRegistry.h"

// Put this in the class definition (.h).
// SingletonRegistry is befriended so that whoever owns the object (e.g. the app's
// AppContext) can construct/destroy it through SingletonRegistry::create/destroy even
// when the constructor and destructor are private — no app type is named here,
// so LITE_SINGLETON stays usable from any layer, including libraries.
#define LITE_SINGLETON_DECLARE_INSTANCE(ClassName)                                                 \
    friend class SingletonRegistry;                                                                          \
    static ClassName *instance();

// Put this in the source file (.cpp).
// Looks the instance up in the process SingletonRegistry (populated by whoever owns the
// object, e.g. the app's AppContext). Falls back to a Meyers static when nothing
// is registered — e.g. in tests, or for a library singleton that has no
// managing owner.
#define LITE_SINGLETON_IMPLEMENT_INSTANCE(ClassName)                                               \
    ClassName *ClassName::instance() {                                                             \
        if (auto *p = SingletonRegistry::instance<ClassName>())                                             \
            return p;                                                                               \
        static ClassName obj;                                                                       \
        return &obj;                                                                                \
    }

#endif // LITE_CORE_SINGLETON_H
