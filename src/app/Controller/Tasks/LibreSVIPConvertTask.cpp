#include "LibreSVIPConvertTask.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QUuid>

#include <utility>

namespace {
    constexpr int kStartTimeoutMs = 10000;
    constexpr int kConvertTimeoutMs = 120000;
    // libresvip-cli asks format-specific questions on stdin (pitch curves,
    // middlewares, export options...). Feeding excess empty lines accepts
    // every default answer regardless of how many prompts a format asks.
    constexpr int kDefaultAnswerLines = 40;
}

LibreSVIPConvertTask::LibreSVIPConvertTask(QString executablePath, QString inputPath)
    : m_executablePath(std::move(executablePath)), m_inputPath(std::move(inputPath)),
      m_outputPath(QDir::temp().filePath(
          QStringLiteral("libresvip_%1.dspx").arg(QUuid::createUuid().toString(QUuid::Id128)))) {
}

const QString &LibreSVIPConvertTask::executablePath() const {
    return m_executablePath;
}

const QString &LibreSVIPConvertTask::inputPath() const {
    return m_inputPath;
}

const QString &LibreSVIPConvertTask::outputPath() const {
    return m_outputPath;
}

DspxParseResult LibreSVIPConvertTask::takeResult() {
    return std::move(m_result);
}

void LibreSVIPConvertTask::runTask() {
    TaskStatus status;
    status.title = QCoreApplication::translate("LibreSVIPConvertTask", "Importing Project");
    status.message =
        QCoreApplication::translate("LibreSVIPConvertTask", "Converting project with LibreSVIP...");
    status.isIndetermine = true;
    setStatus(status);

    if (m_executablePath.isEmpty()) {
        m_result.errorMessage = QCoreApplication::translate(
            "LibreSVIPConvertTask",
            "LibreSVIP executable not found. Install libresvip-cli and set its path.");
        return;
    }

    QProcess process;
    process.setProgram(m_executablePath);
    process.setArguments(
        {QStringLiteral("proj"), QStringLiteral("convert"), m_inputPath, m_outputPath});
    process.start();
    if (!process.waitForStarted(kStartTimeoutMs)) {
        m_result.errorMessage =
            QCoreApplication::translate("LibreSVIPConvertTask", "Failed to start LibreSVIP: %1")
                .arg(process.errorString());
        return;
    }
    // Accept every default answer for the interactive prompts.
    process.write(QByteArray(kDefaultAnswerLines, '\n'));
    process.closeWriteChannel();
    if (!process.waitForFinished(kConvertTimeoutMs)) {
        process.kill();
        process.waitForFinished(3000);
        m_result.errorMessage =
            QCoreApplication::translate("LibreSVIPConvertTask", "LibreSVIP conversion timed out");
        return;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const auto output = QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
        m_result.errorMessage =
            QCoreApplication::translate("LibreSVIPConvertTask", "LibreSVIP conversion failed: %1")
                .arg(output.right(200));
        return;
    }

    QFile outputFile(m_outputPath);
    if (!outputFile.open(QIODevice::ReadOnly)) {
        m_result.errorMessage = QCoreApplication::translate("LibreSVIPConvertTask",
                                                            "Failed to read LibreSVIP output: %1")
                                    .arg(outputFile.errorString());
        return;
    }
    m_result = DspxProjectParser::parse(outputFile.readAll());
    outputFile.close();
    outputFile.remove();
}
