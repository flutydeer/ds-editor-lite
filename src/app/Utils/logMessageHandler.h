#ifndef DS_EDITOR_LITE_LOGMESSAGEHANDLER_H
#define DS_EDITOR_LITE_LOGMESSAGEHANDLER_H
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <ostream>
#include <QDir>

void logMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    QString logFolder = QCoreApplication::applicationDirPath() + QDir::separator() + "log";
    if (!QDir(logFolder).exists()) {
        QDir().mkdir(logFolder);
    }
    QString logFileName = logFolder + QDir::separator() +
                          QDateTime::currentDateTime().toString("yyyy-MM-dd") + ".log";
    QFile logFile(logFileName);
    logFile.open(QIODevice::WriteOnly | QIODevice::Append);

    // 将消息输出到控制台
    QTextStream consoleStream(stdout);
    consoleStream << msg << Qt::endl;

    // 根据消息类型选择输出格式
    QTextStream fileStream(&logFile);
    switch (type) {
        case QtDebugMsg:
            fileStream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                       << " [DEBUG] " << msg << "\n"
                       << " function: " << context.function << Qt::endl;
            break;
        case QtInfoMsg:
            fileStream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << " [INFO] "
                       << msg << "\n"
                       << " function: " << context.function << Qt::endl;
            break;
        case QtWarningMsg:
            fileStream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                       << " [WARNING] " << msg << "\n"
                       << " function: " << context.function << Qt::endl;
            break;
        case QtCriticalMsg:
            fileStream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                       << " [CRITICAL] " << msg << "\n"
                       << " function: " << context.function << Qt::endl;
            break;
        case QtFatalMsg:
            fileStream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                       << " [FATAL] " << msg << "\n"
                       << " function: " << context.function << Qt::endl;
            abort();
        default:
            fileStream << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                       << " [UNKNOWN] " << msg << "\n"
                       << " function: " << context.function << Qt::endl;
            break;
    }

    logFile.close();
}
#endif // DS_EDITOR_LITE_LOGMESSAGEHANDLER_H
