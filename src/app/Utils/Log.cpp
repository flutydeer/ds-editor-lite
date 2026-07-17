//
// Created by fluty on 24-8-18.
//

#include "Log.h"

#include "LogBus.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QScopeGuard>
#include <QSysInfo>

QString Log::LogMessage::toPlainText() const {
    return QString("%1 %2 [%3] %4")
        .arg(time, padText(tag, consoleTagWidth), levelText(level), text);
}

QString Log::LogMessage::toConsoleText() const {
    const auto levelStr = colorizeHighlightText(level, QString(" %1 ").arg(levelText(level)));
    const auto textStr = colorizeText(level, text);
    return QString("%1 %2 %3 %4").arg(time, padText(tag, consoleTagWidth), levelStr, textStr);
}

QString Log::LogMessage::levelText(const LogLevel level) {
    switch (level) {
        case Debug:
            return QStringLiteral("D");
        case Info:
            return QStringLiteral("I");
        case Warning:
            return QStringLiteral("W");
        case Error:
            return QStringLiteral("E");
        case Fatal:
            return QStringLiteral("F");
    }
    return QStringLiteral("?");
}

QString Log::LogMessage::padText(const QString &text, const int spaces) {
    // Pad with spaces to the field width; truncate if longer
    return text.leftJustified(spaces, ' ', true);
}

Log::Log() {
    m_logFileName = QDateTime::currentDateTime().toString("yyyy_MM_dd_hh_mm_ss") + ".log";
}

Log::~Log() = default;

LITE_SINGLETON_IMPLEMENT_INSTANCE(Log)

void Log::handler(const QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    if (msg.startsWith("QWindowsWindow::setGeometry: Unable to set geometry") ||
        msg.startsWith("skipping QEventPoint(id=1 ts=0 pos=0,0"))
        return;

    auto level = Info;
    switch (type) {
        case QtDebugMsg:
            level = Debug;
            break;
        case QtInfoMsg:
            level = Info;
            break;
        case QtWarningMsg:
            level = Warning;
            break;
        case QtCriticalMsg:
            level = Error;
            break;
        case QtFatalMsg:
            level = Fatal;
            break;
    }
    const LogMessage message(timeStr(), level, QFileInfo(context.file).baseName(), msg);
    instance()->log(message);
    if (type == QtFatalMsg)
        abort();
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
    i(tag, "--------- System Info End ---------");
}

void Log::setConsoleLogLevel(const LogLevel level) {
    const auto self = instance();
    QMutexLocker lock(&self->m_mutex);
    self->m_consoleLogLevel = level;
}

void Log::setConsoleTagFilter(const QStringList &tags) {
    const auto self = instance();
    QMutexLocker lock(&self->m_mutex);
    self->m_tagFilter = tags;
}

void Log::setLogDirectory(const QString &directory) {
    if (!QDir().mkpath(directory)) {
        e(QStringLiteral("Log"), "Unable to create log directory: " + directory);
        return;
    }

    const auto self = instance();
    bool opened = false;
    QString logFilePath;
    {
        QMutexLocker lock(&self->m_mutex);
        removeOldLogFiles(QDir(directory));
        self->m_logDirectory = directory;
        self->m_fileStream.setDevice(nullptr);
        self->m_logFile.close();
        logFilePath = QDir(directory).filePath(self->m_logFileName);
        self->m_logFile.setFileName(logFilePath);
        opened = self->m_logFile.open(QIODevice::WriteOnly | QIODevice::Append);
        if (opened)
            self->m_fileStream.setDevice(&self->m_logFile);
        self->m_logToFile = opened;
    }
    // Report the failure outside the lock, since logging acquires the mutex again
    if (!opened)
        e(QStringLiteral("Log"), "Unable to open log file: " + logFilePath);
}

void Log::d(const QString &tag, const QString &msg) {
    instance()->log(LogMessage(timeStr(), Debug, tag, msg));
}

void Log::i(const QString &tag, const QString &msg) {
    instance()->log(LogMessage(timeStr(), Info, tag, msg));
}

void Log::w(const QString &tag, const QString &msg) {
    instance()->log(LogMessage(timeStr(), Warning, tag, msg));
}

void Log::e(const QString &tag, const QString &msg) {
    instance()->log(LogMessage(timeStr(), Error, tag, msg));
}

void Log::f(const QString &tag, const QString &msg) {
    instance()->log(LogMessage(timeStr(), Fatal, tag, msg));
    abort();
}

QString Log::timeStr() {
    return QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
}

QString Log::colorizeText(const LogLevel level, const QString &text) {
    if (level == Debug)
        return QString("\033[0m\033[32m%1\033[0m").arg(text);
    if (level == Info)
        return QString("\033[0m\033[34m%1\033[0m").arg(text);
    if (level == Warning)
        return QString("\033[0m\033[33m%1\033[0m").arg(text);
    return QString("\033[0m\033[31m%1\033[0m").arg(text); // Error or Fatal
}

QString Log::colorizeHighlightText(const LogLevel level, const QString &text) {
    if (level == Debug)
        return QString("\033[0m\033[42;30m%1\033[0m").arg(text);
    if (level == Info)
        return QString("\033[0m\033[44;30m%1\033[0m").arg(text);
    if (level == Warning)
        return QString("\033[0m\033[43;30m%1\033[0m").arg(text);
    return QString("\033[0m\033[41;30m%1\033[0m").arg(text); // Error or Fatal
}

void Log::removeOldLogFiles(const QDir &dir) {
    // Log file names are timestamps, so keep the newest ones and remove the rest.
    // Reserve one slot for the log file about to be created.
    const auto logFiles = dir.entryInfoList({"*.log"}, QDir::Files, QDir::Time);
    for (qsizetype i = maxLogFiles - 1; i < logFiles.size(); i++)
        QFile::remove(logFiles[i].absoluteFilePath());
}

bool Log::canLogToConsole(const LogMessage &message) const {
    if (m_consoleLogLevel > message.level)
        return false;

    if (m_tagFilter.isEmpty())
        return true;

    return m_tagFilter.contains(message.tag);
}

void Log::log(const LogMessage &message) {
    // Guard against reentrancy: a Qt message emitted while writing the log (e.g. a QTextStream
    // or QFile warning) would re-enter through the message handler and deadlock on m_mutex
    static thread_local bool isLogging = false;
    if (isLogging)
        return;
    isLogging = true;
    const auto guard = qScopeGuard([] { isLogging = false; });

    {
        QMutexLocker lock(&m_mutex);
        if (canLogToConsole(message)) {
            QTextStream consoleStream(stdout);
            consoleStream.setEncoding(QStringConverter::System);
            consoleStream << message.toConsoleText() << Qt::endl;
        }

        if (m_logToFile && m_logFile.isOpen())
            m_fileStream << message.toPlainText() << Qt::endl; // Qt::endl flushes the stream
    }

    // Forward the unfiltered message to in-app viewers (LogBus has its own lock)
    LogBus::instance()->append(message);
}
