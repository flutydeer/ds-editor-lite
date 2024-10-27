//
// Created by fluty on 24-8-18.
//

#include "Log.h"

#include "Modules/Inference/Utils/DmlUtils.h"

#include <QTextStream>
#include <QDir>

QString Log::LogMessage::toPlainText() const {
    return QString("%1 %2 [%3] %4")
        .arg(time, padText(tag, consoleTagWidth), levelText(level), text);
}

QString Log::LogMessage::toConsoleText() const {
    auto levelStr = colorizeHighlightText(level, QString(" %1 ").arg(levelText(level)));
    auto textStr = colorizeText(level, text);
    return QString("%1 %2 %3 %4").arg(time, padText(tag, consoleTagWidth), levelStr, textStr);
}

QString Log::LogMessage::levelText(LogLevel level) {
    const QStringList levels{"D", "I", "W", "E", "F"};
    return levels[level];
}

QString Log::LogMessage::padText(const QString &text, int spaces) {
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

Log::Log() {
    m_logFileName = QDateTime::currentDateTime().toString("yyyy_MM_dd_hh_mm_ss") + ".log";
}

void Log::handler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    if (msg.startsWith("QWindowsWindow::setGeometry: Unable to set geometry") ||
        msg.startsWith("skipping QEventPoint(id=1 ts=0 pos=0,0"))
        return;

    LogMessage message;
    message.time = timeStr();
    message.tag = QFileInfo(context.file).baseName();
    message.text = msg;
    // auto methodName = QString("[%1]").arg(prettyMethodName(context.function));
    if (type == QtDebugMsg) {
        message.level = Debug;
        instance()->log(message);
    } else if (type == QtInfoMsg) {
        message.level = Info;
        instance()->log(message);
    } else if (type == QtWarningMsg) {
        message.level = Warning;
        instance()->log(message);
    } else if (type == QtCriticalMsg) {
        message.level = Error;
        instance()->log(message);
    } else if (type == QtFatalMsg) {
        message.level = Fatal;
        instance()->log(message);
        abort();
    }
}

void Log::logSystemInfo() {
    const auto tag = QStringLiteral("Log");
    i(tag, "-------- System Info Begin --------");
    i(tag, "Build CPU Architecture: " + QSysInfo::buildCpuArchitecture());
    i(tag, "Current CPU Architecture: " + QSysInfo::currentCpuArchitecture());
    i(tag, "Product Type: " + QSysInfo::productType());
    i(tag, "Product Name: " + QSysInfo::prettyProductName());
    i(tag, "Product Version: " + QSysInfo::productVersion());
    i(tag, "Kernel Type: " + QSysInfo::kernelType());
    i(tag, "Kernel Version: " + QSysInfo::kernelVersion());
    // i(tag, "Host Name: " + QSysInfo::machineHostName());
    i(tag, "--------- System Info End ---------");
}

void Log::logGpuInfo() {
    qInfo() << "-------- GPU Info Begin --------";
    for (const auto &gpu : DmlUtils::getDirectXGPUs()) {
        qInfo() << gpu.index << gpu.description;
    }
    qInfo() << "--------- GPU Info End ---------";
}

void Log::setConsoleLogLevel(LogLevel level) {
    instance()->m_consoleLogLevel = level;
}

void Log::setConsoleTagFilter(const QStringList &tags) {
    instance()->m_tagFilter = tags;
}

void Log::setLogDirectory(const QString &directory) {
    auto dir = QDir(directory);
    if (!dir.exists()) {
        if (!dir.mkdir(directory))
            qFatal("Unable to create directory %s", directory.toUtf8().data());
    }
    instance()->m_logDirectory = directory;
    instance()->m_logToFile = true;
}

void Log::d(const QString &tag, const QString &msg) {
    const LogMessage message(timeStr(), Debug, tag, msg);
    instance()->log(message);
}

void Log::i(const QString &tag, const QString &msg) {
    const LogMessage message(timeStr(), Info, tag, msg);
    instance()->log(message);
}

void Log::w(const QString &tag, const QString &msg) {
    const LogMessage message(timeStr(), Warning, tag, msg);
    instance()->log(message);
}

void Log::e(const QString &tag, const QString &msg) {
    const LogMessage message(timeStr(), Error, tag, msg);
    instance()->log(message);
}

void Log::f(const QString &tag, const QString &msg) {
    const LogMessage message(timeStr(), Fatal, tag, msg);
    instance()->log(message);
}

QString Log::timeStr() {
    return QDateTime::currentDateTime().toString("hh:mm:ss");
}

QString Log::colorizeText(LogLevel level, const QString &text) {
    if (level == Debug)
        return QString("\033[0m\033[32m%1\033[0m").arg(text);
    if (level == Info)
        return QString("\033[0m\033[34m%1\033[0m").arg(text);
    if (level == Warning)
        return QString("\033[0m\033[33m%1\033[0m").arg(text);
    return QString("\033[0m\033[31m%1\033[0m").arg(text); // Error or Fatal
}

QString Log::colorizeHighlightText(LogLevel level, const QString &text) {
    if (level == Debug)
        return QString("\033[0m\033[42;30m%1\033[0m").arg(text);
    if (level == Info)
        return QString("\033[0m\033[44;30m%1\033[0m").arg(text);
    if (level == Warning)
        return QString("\033[0m\033[43;30m%1\033[0m").arg(text);
    return QString("\033[0m\033[41;30m%1\033[0m").arg(text); // Error or Fatal
}

bool Log::canLogToConsole(const LogMessage &message) {
    if (m_consoleLogLevel > message.level)
        return false;

    if (m_tagFilter.isEmpty())
        return true;

    return std::any_of(m_tagFilter.begin(), m_tagFilter.end(),
                       [&](const QString &tag) { return tag == message.tag; });
}

void Log::log(const LogMessage &message) {
    QMutexLocker lock(&m_mutex);
    QTextStream consoleStream(stdout);
    consoleStream.setEncoding(QStringConverter::System);

    if (canLogToConsole(message))
        consoleStream << message.toConsoleText() << Qt::endl;

    if (m_logToFile) {
        QString logFilePath = m_logDirectory + QDir::separator() + m_logFileName;
        QFile logFile(logFilePath);
        logFile.open(QIODevice::WriteOnly | QIODevice::Append);
        QTextStream fileStream(&logFile);
        fileStream << message.toPlainText() << Qt::endl;
        logFile.close();
    }
}

// QString Logger::prettyMethodName(const char *function) {
//     return QString(function)
//         .remove(" __cdecl")
//         .remove(" __stdcall")
//         .remove(" __fastcall")
//         .remove(" __thiscall");
// }
