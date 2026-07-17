#ifndef LOGBUS_H
#define LOGBUS_H

#include "Log.h"

#include <QMutex>
#include <QObject>

Q_DECLARE_METATYPE(Log::LogMessage)

/// In-memory log sink for in-app log viewers (e.g. LogWindow).
///
/// Log::log() forwards every message here unfiltered; viewers connect to
/// messageLogged() (use a queued connection from the UI thread, since messages
/// may arrive from any thread) and can call recentMessages() to backfill
/// history from the bounded ring buffer when they open.
class LogBus final : public QObject {
    Q_OBJECT

private:
    LogBus() = default;
    ~LogBus() override = default;

public:
    // Plain Meyers singleton: LogBus is logging infrastructure below AppContext
    static LogBus *instance();
    Q_DISABLE_COPY_MOVE(LogBus)

    /// Maximum number of messages kept for backfill
    static constexpr int bufferSize = 10000;

    void append(const Log::LogMessage &message);
    [[nodiscard]] QList<Log::LogMessage> recentMessages() const;

signals:
    void messageLogged(const Log::LogMessage &message);

private:
    mutable QMutex m_mutex;
    QList<Log::LogMessage> m_buffer;
};

#endif // LOGBUS_H
