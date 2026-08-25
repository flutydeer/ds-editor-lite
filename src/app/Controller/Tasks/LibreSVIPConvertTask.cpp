#include "LibreSVIPConvertTask.h"

#include <QCoreApplication>

#include <lite/ProjectConverters/LibreSVIPConverter.h>

#include <utility>

LibreSVIPConvertTask::LibreSVIPConvertTask(QString executablePath, QString inputPath)
    : m_executablePath(std::move(executablePath)), m_inputPath(std::move(inputPath)) {
}

const QString &LibreSVIPConvertTask::executablePath() const {
    return m_executablePath;
}

const QString &LibreSVIPConvertTask::inputPath() const {
    return m_inputPath;
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

    const auto converted = LibreSVIPConverter::convertToDspx(m_executablePath, m_inputPath);
    if (!converted.success()) {
        m_result.errorMessage = converted.errorMessage;
        return;
    }
    m_result = DspxProjectParser::parse(converted.dspxData);
}
