//
// Created by fluty on 24-8-18.
//

#include "Logger.h"

#include <QTextStream>
#include <QDir>


void Logger::handler(QtMsgType type, const QMessageLogContext&context, const QString&msg) {
    auto logFolder = QCoreApplication::applicationDirPath() + QDir::separator() + "log";
    auto dir = QDir(logFolder);
    if (!dir.exists()) {
        if(!dir.mkdir(logFolder))
            qFatal("Unable to create directory %s", logFolder.toUtf8().data());
    }
    auto logFileName = QDateTime::currentDateTime().toString("yyyy_MM_dd_hh_mm_ss") + ".log";
    QString logFilePath = logFolder + QDir::separator() + logFileName;
    QFile logFile(logFilePath);
    logFile.open(QIODevice::WriteOnly | QIODevice::Append);

    // 将消息输出到控制台
    QTextStream consoleStream(stdout);
    consoleStream.setEncoding(QStringConverter::System);

    // 根据消息类型选择输出格式
    QTextStream fileStream(&logFile);
    QString message = messageTimeStr();
    switch (type) {
        case QtInfoMsg:
            message += "[I] " + msg;
            fileStream << message << Qt::endl;
            consoleStream << colorMessage(Info, message) << Qt::endl;
            break;
        case QtDebugMsg:
            message += "[D] " + msg;
            fileStream << message << Qt::endl;
            consoleStream << colorMessage(Debug, message) << Qt::endl;
            break;
        case QtWarningMsg:
            message += "[W] " + msg;
            fileStream << message << Qt::endl;
            consoleStream << colorMessage(Warning, message) << Qt::endl;
            break;
        case QtCriticalMsg:
            message += "[E] " + msg;
            fileStream << message << Qt::endl;
            consoleStream << colorMessage(Error, message) << Qt::endl;
            break;
        case QtFatalMsg:
            message += "[F] " + msg;
            fileStream << message << Qt::endl;
            consoleStream << colorMessage(Error, message) << Qt::endl;
            abort();
        default:
            message += "[?] " + msg;
            fileStream << message << Qt::endl;
            consoleStream << colorMessage(Info, message) << Qt::endl;
            break;
    }

    logFile.close();
}

QString Logger::messageTimeStr() {
    return QDateTime::currentDateTime().toString("hh:mm:ss ");
}

QString Logger::colorMessage(LogLevel level, const QString&message) {
    if (level == Info) {
        return message;
    }
    if (level == Debug) {
        return QString("\033[0m\033[1;32m%1\033[0m").arg(message);
    }
    if (level == Warning) {
        return QString("\033[0m\033[1;33m%1\033[0m").arg(message);
    }
    if (level == Error) {
        return QString("\033[0m\033[1;31m%1\033[0m").arg(message);
    }
    return message;
}
