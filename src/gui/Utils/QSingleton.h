#ifndef DS_EDITOR_LITE_QSINGLETON_H
#define DS_EDITOR_LITE_QSINGLETON_H

#include <QMutex>
#include <QScopedPointer>

template <typename T>
class QSingleton {
public:
    static T *instance() {
        static QScopedPointer<T> m_instance;
        static QMutex m_mutex;
        if (Q_UNLIKELY(!m_instance)) {
            m_mutex.lock();
            if (!m_instance) {
                m_instance.reset(new T());
            }
            m_mutex.unlock();
        }
        return m_instance.data();
    }
};

// 带参数的单例模式
template <typename T, typename... Args>
class QSingletonWithArgs {
public:
    static T *instance(Args... args) {
        static QScopedPointer<T> m_instance;
        static QMutex m_mutex;
        if (Q_UNLIKELY(!m_instance)) {
            m_mutex.lock();
            if (!m_instance) {
                m_instance.reset(new T(args...));
            }
            m_mutex.unlock();
        }
        return m_instance.data();
    }
};

#endif // DS_EDITOR_LITE_QSINGLETON_H
