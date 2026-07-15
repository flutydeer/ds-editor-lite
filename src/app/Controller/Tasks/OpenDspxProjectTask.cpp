#include "OpenDspxProjectTask.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>

#include <utility>

namespace {
    constexpr qint64 readChunkSize = 256 * 1024;
}

OpenDspxProjectTask::OpenDspxProjectTask(QString filePath, const quint64 requestId)
    : m_filePath(std::move(filePath)), m_requestId(requestId) {
    TaskStatus status;
    status.title = QCoreApplication::translate("OpenDspxProjectTask", "Opening Project");
    status.message = QCoreApplication::translate("OpenDspxProjectTask", "Reading project file...");
    status.isIndetermine = false;
    setStatus(status);
}

const QString &OpenDspxProjectTask::filePath() const {
    return m_filePath;
}

quint64 OpenDspxProjectTask::requestId() const {
    return m_requestId;
}

qint64 OpenDspxProjectTask::fileSize() const {
    return m_fileSize;
}

qint64 OpenDspxProjectTask::readElapsedMs() const {
    return m_readElapsedMs;
}

qint64 OpenDspxProjectTask::parseElapsedMs() const {
    return m_parseElapsedMs;
}

DspxParseResult OpenDspxProjectTask::takeResult() {
    return std::move(m_result);
}

void OpenDspxProjectTask::runTask() {
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_result.errorMessage =
            QCoreApplication::translate("OpenDspxProjectTask", "Failed to open project file: %1")
                .arg(m_filePath);
        return;
    }

    m_fileSize = file.size();
    QByteArray data;
    data.reserve(m_fileSize);

    QElapsedTimer timer;
    timer.start();
    qint64 bytesRead = 0;
    while (!file.atEnd()) {
        if (isTerminateRequested())
            return;

        const auto chunk = file.read(readChunkSize);
        if (chunk.isEmpty() && file.error() != QFileDevice::NoError) {
            m_result.errorMessage = QCoreApplication::translate("OpenDspxProjectTask",
                                                                "Failed to read project file: %1")
                                        .arg(file.errorString());
            return;
        }
        data.append(chunk);
        bytesRead += chunk.size();

        TaskStatus status;
        status.title = QCoreApplication::translate("OpenDspxProjectTask", "Opening Project");
        status.message =
            QCoreApplication::translate("OpenDspxProjectTask", "Reading project file...");
        status.isIndetermine = m_fileSize <= 0;
        status.progress = m_fileSize > 0 ? static_cast<int>(bytesRead * 100 / m_fileSize) : 0;
        setStatus(status);
    }
    m_readElapsedMs = timer.elapsed();

    if (isTerminateRequested())
        return;

    TaskStatus status;
    status.title = QCoreApplication::translate("OpenDspxProjectTask", "Opening Project");
    status.message = QCoreApplication::translate("OpenDspxProjectTask", "Parsing project file...");
    status.isIndetermine = true;
    setStatus(status);

    timer.restart();
    m_result = DspxProjectParser::parse(data);
    m_parseElapsedMs = timer.elapsed();

    if (isTerminateRequested())
        m_result = {};
}
