#ifndef LITE_CORE_SINGLETON_H
#define LITE_CORE_SINGLETON_H

#include "SingletonRegistry.h"

// Declare in the class definition. Friendship lets SingletonRegistry create and destroy
// instances with private constructors and destructors.
#define LITE_SINGLETON_DECLARE_INSTANCE(ClassName)                                                 \
    friend class SingletonRegistry;                                                                          \
    static ClassName *instance();

// Define in the source file. Look up the registered instance, falling back to a
// function-local static when no owner has registered one.
#define LITE_SINGLETON_IMPLEMENT_INSTANCE(ClassName)                                               \
    ClassName *ClassName::instance() {                                                             \
        if (auto *p = SingletonRegistry::instance<ClassName>())                                             \
            return p;                                                                               \
        static ClassName obj;                                                                       \
        return &obj;                                                                                \
    }

#endif // LITE_CORE_SINGLETON_H
