//
// Created by fluty on 24-8-18.
//

#include "Logger.h"

#include <QTextStream>
#include <QDir>
#include <qmessagebox.h>

QString Logger::LogMessage::toPlainText() const {
    return QString("%1 [%2] [%3] %4")
        .arg(time, padText(tag, consoleTagWidth), levelText(level), text);
}

QString Logger::LogMessage::toConsoleText() const {
    auto levelStr = colorTextHighlight(level, QString(" %1 ").arg(levelText(level)));
    auto textStr = colorText(level, text);
    return QString("%1 %2 %3 %4").arg(time, padText(tag, consoleTagWidth), levelStr, textStr);
}

QString Logger::LogMessage::levelText(LogLevel level) {
    const QStringList levels{"D", "I", "W", "E", "F"};
    return levels[level];
}

QString Logger::LogMessage::padText(const QString &text, int spaces) {
    QString result;
    if (text.length() >= spaces)
        result = text.left(spaces);
    else {
        auto padding = spaces - text.length();
        QString spacingText;
        for (auto i = 0; i < padding; i++)
            spacingText += " ";
        result = text + spacingText;
    }
    return result;
}

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
    if (msg.startsWith("QWindowsWindow::setGeometry: Unable to set geometry"))
        return;

    LogMessage message;
    message.time = timeStr();
    message.text = msg;
    // auto methodName = QString("[%1]").arg(prettyMethodName(context.function));
    if (type == QtDebugMsg) {
        message.level = Debug;
        log(message);
    } else if (type == QtInfoMsg) {
        message.level = Info;
        log(message);
    } else if (type == QtWarningMsg) {
        message.level = Warning;
        log(message);
    } else if (type == QtCriticalMsg) {
        message.level = Error;
        log(message);
    } else if (type == QtFatalMsg) {
        message.level = Fatal;
        log(message);
        abort();
    }
}

void Logger::logSystemInfo() {
    const auto tag = QStringLiteral("Logger");
    i(tag, "-------- System Info Begin --------");
    i(tag, "Build CPU Architecture: " + QSysInfo::buildCpuArchitecture());
    i(tag, "Current CPU Architecture: " + QSysInfo::currentCpuArchitecture());
    i(tag, "Product Type: " + QSysInfo::productType());
    i(tag, "Product Name: " + QSysInfo::prettyProductName());
    i(tag, "Product Version: " + QSysInfo::productVersion());
    i(tag, "Kernel Type: " + QSysInfo::kernelType());
    i(tag, "Kernel Version: " + QSysInfo::kernelVersion());
    i(tag, "Host Name: " + QSysInfo::machineHostName());
    i(tag, "-------- System Info End --------");
}

void Logger::setConsoleLogLevel(LogLevel level) {
    instance()->m_consoleLogLevel = level;
}

void Logger::setConsoleTagFilter(const QStringList &tags) {
    instance()->m_tagFilter = tags;
}

void Logger::d(const QString &tag, const QString &msg) {
    const LogMessage message(timeStr(), Debug, tag, msg);
    log(message);
}

void Logger::i(const QString &tag, const QString &msg) {
    const LogMessage message(timeStr(), Info, tag, msg);
    log(message);
}

void Logger::w(const QString &tag, const QString &msg) {
    const LogMessage message(timeStr(), Warning, tag, msg);
    log(message);
}

void Logger::e(const QString &tag, const QString &msg) {
    const LogMessage message(timeStr(), Error, tag, msg);
    log(message);
}

void Logger::f(const QString &tag, const QString &msg) {
    const LogMessage message(timeStr(), Fatal, tag, msg);
    log(message);
}

QString Logger::timeStr() {
    return QDateTime::currentDateTime().toString("hh:mm:ss");
}

QString Logger::colorText(LogLevel level, const QString &text) {
    if (level == Debug)
        return QString("\033[0m\033[32m%1\033[0m").arg(text);
    if (level == Info)
        return QString("\033[0m\033[34m%1\033[0m").arg(text);
    if (level == Warning)
        return QString("\033[0m\033[33m%1\033[0m").arg(text);
    return QString("\033[0m\033[31m%1\033[0m").arg(text); // Error or Fatal
}

QString Logger::colorTextHighlight(LogLevel level, const QString &text) {
    if (level == Debug)
        return QString("\033[0m\033[42;30m%1\033[0m").arg(text);
    if (level == Info)
        return QString("\033[0m\033[44;30m%1\033[0m").arg(text);
    if (level == Warning)
        return QString("\033[0m\033[43;30m%1\033[0m").arg(text);
    return QString("\033[0m\033[41;30m%1\033[0m").arg(text); // Error or Fatal
}

void Logger::log(const LogMessage &message) {
    QTextStream consoleStream(stdout);
    consoleStream.setEncoding(QStringConverter::System);

    if (canLogToConsole(message))
        consoleStream << message.toConsoleText() << Qt::endl;

    if (instance()->m_logToFile) {
        QString logFilePath =
            instance()->m_logFolder + QDir::separator() + instance()->m_logFileName;
        QFile logFile(logFilePath);
        logFile.open(QIODevice::WriteOnly | QIODevice::Append);
        QTextStream fileStream(&logFile);
        fileStream << message.toPlainText() << Qt::endl;
        logFile.close();
    }
}

bool Logger::canLogToConsole(const LogMessage &message) {
    if (instance()->m_consoleLogLevel > message.level)
        return false;

    if (instance()->m_tagFilter.isEmpty())
        return true;

    return std::any_of(instance()->m_tagFilter.begin(), instance()->m_tagFilter.end(),
                       [&](const QString &tag) { return tag == message.tag; });
}

// QString Logger::prettyMethodName(const char *function) {
//     return QString(function)
//         .remove(" __cdecl")
//         .remove(" __stdcall")
//         .remove(" __fastcall")
//         .remove(" __thiscall");
// }
