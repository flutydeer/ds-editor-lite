//
// Created by fluty on 24-8-18.
//

#include "Logger.h"

#include <QTextStream>
#include <QDir>

Logger::Logger() {
    m_logFolder = QCoreApplication::applicationDirPath() + QDir::separator() + "log";
    m_logFileName = QDateTime::currentDateTime().toString("yyyy_MM_dd_hh_mm_ss") + ".log";

    auto dir = QDir(m_logFolder);
    if (!dir.exists()) {
        if (!dir.mkdir(m_logFolder))
            qFatal("Unable to create directory %s", m_logFolder.toUtf8().data());
    }
}

void Logger::handler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    if (msg.startsWith("QWindowsWindow::setGeometry: Unable to set geometry")) {
        return;
    }

    QTextStream consoleStream(stdout);
    consoleStream.setEncoding(QStringConverter::System);

    QString message = messageTimeStr();
    switch (type) {
        case QtInfoMsg:
            message += "[I] " + msg;
            consoleStream << colorMessage(Info, message) << Qt::endl;
            break;
        case QtDebugMsg:
            message += "[D] " + msg;
            consoleStream << colorMessage(Debug, message) << Qt::endl;
            break;
        case QtWarningMsg:
            message += "[W] " + msg;
            consoleStream << colorMessage(Warning, message) << Qt::endl;
            break;
        case QtCriticalMsg:
            message += "[E] " + msg;
            consoleStream << colorMessage(Error, message) << Qt::endl;
            break;
        case QtFatalMsg:
            message += "[F] " + msg;
            consoleStream << colorMessage(Error, message) << Qt::endl;
            abort();
        default:
            message += "[?] " + msg;
            consoleStream << colorMessage(Info, message) << Qt::endl;
            break;
    }

    if (instance()->m_logToFile) {
        QString logFilePath =
            instance()->m_logFolder + QDir::separator() + instance()->m_logFileName;
        QFile logFile(logFilePath);
        logFile.open(QIODevice::WriteOnly | QIODevice::Append);
        QTextStream fileStream(&logFile);
        fileStream << message << Qt::endl;
        logFile.close();
    }
}

void Logger::logSystemInfo() {
    qInfo() << "-------- System Info Begin --------";
    qInfo() << "Build CPU Architecture:" << QSysInfo::buildCpuArchitecture();
    qInfo() << "Current CPU Architecture:" << QSysInfo::currentCpuArchitecture();
    qInfo() << "Product Type:" << QSysInfo::productType();
    qInfo() << "Product Name:" << QSysInfo::prettyProductName();
    qInfo() << "Product Version:" << QSysInfo::productVersion();
    qInfo() << "Kernel Type:" << QSysInfo::kernelType();
    qInfo() << "Kernel Version:" << QSysInfo::kernelVersion();
    qInfo() << "Host Name:" << QSysInfo::machineHostName();
    qInfo() << "-------- System Info End --------";
}

QString Logger::messageTimeStr() {
    return QDateTime::currentDateTime().toString("hh:mm:ss ");
}

QString Logger::colorMessage(LogLevel level, const QString &message) {
    if (level == Debug) {
        return QString("\033[0m\033[32m%1\033[0m").arg(message);
    }
    if (level == Warning) {
        return QString("\033[0m\033[33m%1\033[0m").arg(message);
    }
    if (level == Error) {
        return QString("\033[0m\033[31m%1\033[0m").arg(message);
    }
    return message; // Info
}
