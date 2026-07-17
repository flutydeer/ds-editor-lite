//
// Created by fluty on 24-8-18.
//

#ifndef LOG_H
#define LOG_H

#include "Singleton.h"

#include <QFile>
#include <QMutex>
#include <QStringList>
#include <QTextStream>
#include <QtLogging>
#include <utility>

class QDir;

class Log {
public:
    enum LogLevel { Debug, Info, Warning, Error, Fatal }; // Green, Blue, Yellow, Red

    class LogMessage {
    public:
        LogMessage() = default;

        LogMessage(QString time, const LogLevel level, QString tag, QString text)
            : time(std::move(time)), level(level), tag(std::move(tag)), text(std::move(text)) {
        }

        [[nodiscard]] QString toPlainText() const;
        [[nodiscard]] QString toConsoleText() const;
        [[nodiscard]] static QString levelText(LogLevel level);
        static QString padText(const QString &text, int spaces);

        QString time;
        LogLevel level = Info;
        QString tag;
        QString text;

        static constexpr int consoleTagWidth = 24;
    };

private:
    Log();
    ~Log();

public:
    LITE_SINGLETON_DECLARE_INSTANCE(Log)
    Q_DISABLE_COPY_MOVE(Log)

public:
    static void handler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
    static void logSystemInfo();
    static void setConsoleLogLevel(LogLevel level);
    static void setConsoleTagFilter(const QStringList &tags);
    static void setLogDirectory(const QString &directory);

    static void d(const QString &tag, const QString &msg);
    static void i(const QString &tag, const QString &msg);
    static void w(const QString &tag, const QString &msg);
    static void e(const QString &tag, const QString &msg);
    /// Logs a fatal message and aborts the process, matching qFatal() semantics
    static void f(const QString &tag, const QString &msg);

private:
    // Retention policy: number of log files kept in the log directory (including the current one)
    static constexpr int maxLogFiles = 10;

    static QString timeStr();
    static QString colorizeText(LogLevel level, const QString &text);
    static QString colorizeHighlightText(LogLevel level, const QString &text);
    static void removeOldLogFiles(const QDir &dir);
    bool canLogToConsole(const LogMessage &message) const;
    void log(const LogMessage &message);

    QMutex m_mutex;
    QString m_logDirectory;
    QString m_logFileName;
    QFile m_logFile;
    QTextStream m_fileStream;
    bool m_logToFile = false;
    LogLevel m_consoleLogLevel = Debug;
    QStringList m_tagFilter;
};


#endif // LOG_H
