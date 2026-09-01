#include <lite/ProjectConverters/LibreSVIPConverter.h>

#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>

namespace {
    constexpr int kStartTimeoutMs = 10000;
    constexpr int kConvertTimeoutMs = 120000;
    constexpr int kDefaultAnswerLines = 40;
}

LibreSVIPConversionResult LibreSVIPConverter::convertToDspx(const QString &executablePath,
                                                            const QString &inputPath) {
    if (executablePath.isEmpty()) {
        return {{}, QCoreApplication::translate(
                        "LibreSVIPConverter",
                        "LibreSVIP executable not found. Install libresvip-cli and set its path.")};
    }

    QTemporaryDir temporaryDir;
    if (!temporaryDir.isValid()) {
        return {{}, QCoreApplication::translate("LibreSVIPConverter",
                                               "Failed to create a temporary directory")};
    }
    const auto outputPath = temporaryDir.filePath(QStringLiteral("converted.dspx"));

    QProcess process;
    process.setProgram(executablePath);
    process.setArguments(
        {QStringLiteral("proj"), QStringLiteral("convert"), inputPath, outputPath});
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    if (!process.waitForStarted(kStartTimeoutMs)) {
        return {{}, QCoreApplication::translate("LibreSVIPConverter",
                                               "Failed to start LibreSVIP: %1")
                        .arg(process.errorString())};
    }

    // LibreSVIP asks format-specific questions on stdin. Empty lines accept its defaults.
    process.write(QByteArray(kDefaultAnswerLines, '\n'));
    process.closeWriteChannel();
    if (!process.waitForFinished(kConvertTimeoutMs)) {
        process.kill();
        process.waitForFinished(3000);
        return {{}, QCoreApplication::translate("LibreSVIPConverter",
                                               "LibreSVIP conversion timed out")};
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const auto output = QString::fromLocal8Bit(process.readAll()).trimmed();
        return {{}, QCoreApplication::translate("LibreSVIPConverter",
                                               "LibreSVIP conversion failed: %1")
                        .arg(output.right(500))};
    }

    QFile outputFile(outputPath);
    if (!outputFile.open(QIODevice::ReadOnly)) {
        return {{}, QCoreApplication::translate("LibreSVIPConverter",
                                               "Failed to read LibreSVIP output: %1")
                        .arg(outputFile.errorString())};
    }
    const auto data = outputFile.readAll();
    if (outputFile.error() != QFileDevice::NoError || data.isEmpty()) {
        return {{}, QCoreApplication::translate("LibreSVIPConverter",
                                               "Failed to read LibreSVIP output: %1")
                        .arg(outputFile.errorString())};
    }
    return {data, {}};
}
