//
// Created by fluty on 24-8-18.
//

#ifndef LOGGER_H
#define LOGGER_H

#include "Singleton.h"

#include <QCoreApplication>

class Logger : public Singleton<Logger> {
public:
    Logger();
    static  void handler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
    static void logSystemInfo();

private:
    enum LogLevel { Info, Debug, Warning, Error }; // White, Green, Yellow, Red

    static QString messageTimeStr();
    static QString colorMessage(LogLevel level, const QString &message);

    QString m_logFolder;
    QString m_logFileName;
};


#endif // LOGGER_H
