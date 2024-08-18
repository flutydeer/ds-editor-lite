//
// Created by fluty on 24-8-18.
//

#ifndef LOGGER_H
#define LOGGER_H

#include <QCoreApplication>

class Logger {
public:
    static void handler(QtMsgType type, const QMessageLogContext&context, const QString&msg);

private:
    enum LogLevel { Info, Debug, Warning, Error }; // White, Green, Yellow, Red

    static QString messageTimeStr();
    static QString colorMessage(LogLevel level, const QString&message);
};


#endif //LOGGER_H
