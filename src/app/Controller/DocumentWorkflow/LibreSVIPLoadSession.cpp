#include "LibreSVIPLoadSession.h"

#include "Controller/Tasks/LibreSVIPConvertTask.h"
#include "Modules/ProjectFormats/LibreSVIPFormatHandler.h"

#include <utility>

LibreSVIPLoadSession::LibreSVIPLoadSession(IProjectFormatHandler *formatHandler, QString filePath,
                                           const ProjectLoadPurpose purpose,
                                           const quint64 requestId, const bool interactive,
                                           const bool importTempo,
                                           const bool importTimeSignature, QObject *parent)
    : OpendspxImportLoadSession(formatHandler, std::move(filePath), purpose, requestId,
                               interactive, importTempo, importTimeSignature, parent) {
}

Task *LibreSVIPLoadSession::createParseTask() {
    return new LibreSVIPConvertTask(LibreSVIPFormatHandler::executablePath(), m_filePath);
}

OpendspxImportLoadSession::ParseOutcome LibreSVIPLoadSession::takeParsedModel(Task *task) {
    auto *convertTask = static_cast<LibreSVIPConvertTask *>(task);
    auto result = convertTask->takeResult();
    return {std::move(result.model), result.errorMessage};
}
