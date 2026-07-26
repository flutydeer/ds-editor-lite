#include "LogBus.h"

LITE_SINGLETON_IMPLEMENT_INSTANCE(LogBus)

void LogBus::append(const Log::LogMessage &message) {
    {
        QMutexLocker lock(&m_mutex);
        m_buffer.append(message);
        if (m_buffer.size() > bufferSize)
            m_buffer.removeFirst();
    }
    emit messageLogged(message);
}

QList<Log::LogMessage> LogBus::recentMessages() const {
    QMutexLocker lock(&m_mutex);
    return m_buffer;
}
