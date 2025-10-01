//
// Created by fluty on 2024/1/31.
// Modified by Jobsecond on 2025/9/6.
//

#ifndef SINGLETON_H
#define SINGLETON_H


// Put this in the class definition (.h)
#define LITE_SINGLETON_DECLARE_INSTANCE(ClassName) static ClassName *instance();

// Put this in the source file (.cpp)
#define LITE_SINGLETON_IMPLEMENT_INSTANCE(ClassName)                                               \
    ClassName *ClassName::instance() {                                                             \
        static ClassName obj;                                                                      \
        return &obj;                                                                               \
    }


/// Usage:
///
/// header file - MySingleton.h
/// @code
/// class MySingleton {
/// private:
///     MySingleton();
///     ~MySingleton();
///
/// public:
///     // declare instance() method
///     LITE_SINGLETON_DECLARE_INSTANCE(MySingleton)
///
///     // disable copy/move, or just use Q_DISABLE_COPY_MOVE(MySingleton) if using Qt
///     MySingleton(const MySingleton &) = delete;
///     MySingleton &operator=(const MySingleton &) = delete;
///     MySingleton(MySingleton &&) = delete;
///     MySingleton &operator=(MySingleton &&) = delete;
/// };
/// @endcode
///
/// source file - MySingleton.cpp
/// @code
/// #include "MySingleton.h"
///
/// MySingleton::MySingleton() {
///     // constructor
/// }
///
/// MySingleton::~MySingleton() {
///     // destructor
/// }
///
/// // implement instance() method: do not forget this in .cpp
/// LITE_SINGLETON_IMPLEMENT_INSTANCE(MySingleton)
/// @endcode


#endif // SINGLETON_H
