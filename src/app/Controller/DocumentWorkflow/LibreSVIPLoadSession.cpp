#include "LibreSVIPLoadSession.h"

#include "Controller/Tasks/LibreSVIPConvertTask.h"
#include "Modules/ProjectFormats/LibreSVIPFormatHandler.h"

#include <utility>

LibreSVIPLoadSession::LibreSVIPLoadSession(IProjectFormatHandler *formatHandler, QString filePath,
                                           const ProjectLoadPurpose purpose,
                                           const quint64 requestId, QObject *parent)
    : OpendspxImportLoadSession(formatHandler, std::move(filePath), purpose, requestId, parent) {
}

Task *LibreSVIPLoadSession::createParseTask() {
    return new LibreSVIPConvertTask(LibreSVIPFormatHandler::executablePath(), m_filePath);
}

OpendspxImportLoadSession::ParseOutcome LibreSVIPLoadSession::takeParsedModel(Task *task) {
    auto *convertTask = static_cast<LibreSVIPConvertTask *>(task);
    auto result = convertTask->takeResult();
    return {std::move(result.model), result.errorMessage};
}
