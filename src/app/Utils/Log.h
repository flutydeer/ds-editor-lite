//
// Created by fluty on 24-8-18.
//

#ifndef LOG_H
#define LOG_H

#define qStrNum(num) QString::number(num)

#define qStrRect(r)                                                                                \
    QString("QRect (%1, %2) %3x%4")                                                                \
        .arg(qStrNum(r.left()), qStrNum(r.top()), qStrNum(r.width()), qStrNum(r.height()))

#define qStrRectF(r)                                                                               \
    QString("QRectF (%1, %2) %3x%4")                                                               \
        .arg(qStrNum(r.left()), qStrNum(r.top()), qStrNum(r.width()), qStrNum(r.height()))

#include "Singleton.h"

#include <QCoreApplication>
#include <QMutex>
#include <utility>

class Log : public Singleton<Log> {
public:
    enum LogLevel { Debug, Info, Warning, Error, Fatal }; // Green, Blue, Yellow, Red

    class LogMessage {
    public:
        LogMessage() = default;
        LogMessage(QString time, QString tag, QString text)
            : time(std::move(time)), tag(std::move(tag)), text(std::move(text)){};
        LogMessage(QString time, LogLevel level, QString tag, QString text)
            : time(std::move(time)), level(level), tag(std::move(tag)), text(std::move(text)){};

        [[nodiscard]] QString toPlainText() const;
        [[nodiscard]] QString toConsoleText() const;
        [[nodiscard]] static QString levelText(LogLevel level);
        static QString padText(const QString &text, int spaces);

        QString time;
        LogLevel level = Info;
        QString tag;
        QString text;

        const int consoleTagWidth = 24;
    };

    Log();
    static void handler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
    static void logSystemInfo();
    static void logGpuInfo();
    static void setConsoleLogLevel(LogLevel level);
    static void setConsoleTagFilter(const QStringList &tags);

    static void d(const QString &tag, const QString &msg);
    static void i(const QString &tag, const QString &msg);
    static void w(const QString &tag, const QString &msg);
    static void e(const QString &tag, const QString &msg);
    static void f(const QString &tag, const QString &msg);

private:
    static QString timeStr();
    static QString colorizeText(LogLevel level, const QString &text);
    static QString colorizeHighlightText(LogLevel level, const QString &text);
    // static QString prettyMethodName(const char *function);
    bool canLogToConsole(const LogMessage &message);
    void log(const LogMessage &message);

    QMutex m_mutex;
    QString m_logFolder;
    QString m_logFileName;
    bool m_logToFile = false;
    LogLevel m_consoleLogLevel = Debug;
    QStringList m_tagFilter;
};


#endif // LOG_H
